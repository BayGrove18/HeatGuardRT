#include "upgrade.h"

#include "task.h"

#include "boot_handoff.h"
#include "crc32.h"
#include "handoff_contract.h"
#include "heatguard_config.h"
#include "upgrade_transport.h"
#include "w25q64.h"

#define UPGRADE_SYNC_0 0xA5U
#define UPGRADE_SYNC_1 0x5AU
#define UPGRADE_FRAME_BEGIN 0x01U
#define UPGRADE_FRAME_DATA 0x02U
#define UPGRADE_FRAME_FINISH 0x03U
#define UPGRADE_FRAME_ABORT 0x04U

typedef enum {
    PARSER_SYNC_0 = 0,
    PARSER_SYNC_1,
    PARSER_TYPE,
    PARSER_LENGTH_LO,
    PARSER_LENGTH_HI,
    PARSER_PAYLOAD,
    PARSER_CRC_LO,
    PARSER_CRC_HI
} ParserState;

typedef struct {
    ParserState state;
    uint8_t type;
    uint16_t length;
    uint16_t index;
    uint16_t crc16;
    uint16_t expected_crc16;
    uint8_t payload[HEATGUARD_UPGRADE_MAX_PAYLOAD];
} UpgradeParser;

typedef struct {
    uint8_t active;
    uint8_t verified;
    uint16_t expected_sequence;
    uint32_t image_size;
    uint32_t image_crc32;
    uint32_t image_version;
    uint32_t transaction_id;
    uint32_t received_size;
    uint32_t running_crc32;
} UpgradeSession;

static UpgradeParser parser;
static UpgradeSession session;
static UpgradeReadyCallback upgrade_ready_callback;

static uint16_t crc16_update(uint16_t crc, uint8_t value)
{
    uint8_t bit;

    crc ^= (uint16_t)value << 8;
    for (bit = 0U; bit < 8U; ++bit) {
        crc = (crc & 0x8000U) != 0U ? (uint16_t)((crc << 1) ^ 0x1021U) :
                                      (uint16_t)(crc << 1);
    }
    return crc;
}

static uint32_t read_u32_le(const uint8_t *data)
{
    return (uint32_t)data[0] |
           ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}

static uint16_t read_u16_le(const uint8_t *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static void session_abort(void)
{
    session.active = 0U;
    session.verified = 0U;
    session.received_size = 0U;
    session.expected_sequence = 0U;
}

static uint8_t verify_staged_image(void)
{
    uint8_t buffer[64];
    uint32_t offset = 0U;
    uint32_t crc = crc32_init();

    while (offset < session.image_size) {
        uint32_t chunk = session.image_size - offset;

        if (chunk > sizeof(buffer)) {
            chunk = sizeof(buffer);
        }
        if (w25q64_read(HEATGUARD_STAGE_IMAGE_OFFSET + offset, buffer, chunk) == 0U) {
            return 0U;
        }
        crc = crc32_update(crc, buffer, chunk);
        offset += chunk;
    }
    return (uint8_t)(crc32_finalize(crc) == session.image_crc32);
}

static void handle_frame(void)
{
    if (parser.type == UPGRADE_FRAME_ABORT) {
        session_abort();
        return;
    }

    if (parser.type == UPGRADE_FRAME_BEGIN && parser.length == 12U) {
        uint32_t image_size = read_u32_le(&parser.payload[0]);

        if (image_size == 0U || image_size > HEATGUARD_STAGE_IMAGE_MAX_SIZE ||
            w25q64_erase_range(HEATGUARD_STAGE_IMAGE_OFFSET, image_size) == 0U) {
            session_abort();
            return;
        }
        session.active = 1U;
        session.verified = 0U;
        session.expected_sequence = 0U;
        session.image_size = image_size;
        session.image_crc32 = read_u32_le(&parser.payload[4]);
        session.image_version = read_u32_le(&parser.payload[8]);
        session.transaction_id = xTaskGetTickCount();
        session.received_size = 0U;
        session.running_crc32 = crc32_init();
        return;
    }

    if (parser.type == UPGRADE_FRAME_DATA && parser.length >= 3U && session.active != 0U) {
        uint16_t sequence = read_u16_le(&parser.payload[0]);
        uint16_t data_length = (uint16_t)(parser.length - 2U);

        if (sequence != session.expected_sequence ||
            session.received_size + data_length > session.image_size ||
            w25q64_write(HEATGUARD_STAGE_IMAGE_OFFSET + session.received_size,
                         &parser.payload[2], data_length) == 0U) {
            session_abort();
            return;
        }
        session.running_crc32 = crc32_update(session.running_crc32,
                                             &parser.payload[2], data_length);
        session.received_size += data_length;
        ++session.expected_sequence;
        return;
    }

    if (parser.type == UPGRADE_FRAME_FINISH && parser.length == 0U &&
        session.active != 0U && session.received_size == session.image_size &&
        crc32_finalize(session.running_crc32) == session.image_crc32) {
        BootManifest manifest;

        if (verify_staged_image() == 0U) {
            session_abort();
            return;
        }
        handoff_manifest_build(&manifest, session.transaction_id, session.image_size,
                               session.image_crc32, session.image_version);
        if (boot_handoff_publish(&manifest) != 0U) {
            session.active = 0U;
            session.verified = 1U;
            if (upgrade_ready_callback != NULL) {
                upgrade_ready_callback();
            }
        } else {
            session_abort();
        }
        return;
    }

    session_abort();
}

static void parser_reset(void)
{
    parser.state = PARSER_SYNC_0;
    parser.index = 0U;
    parser.length = 0U;
}

static void parser_feed(uint8_t value)
{
    switch (parser.state) {
    case PARSER_SYNC_0:
        parser.state = value == UPGRADE_SYNC_0 ? PARSER_SYNC_1 : PARSER_SYNC_0;
        break;
    case PARSER_SYNC_1:
        parser.state = value == UPGRADE_SYNC_1 ? PARSER_TYPE : PARSER_SYNC_0;
        break;
    case PARSER_TYPE:
        parser.type = value;
        parser.crc16 = crc16_update(0xFFFFU, value);
        parser.state = PARSER_LENGTH_LO;
        break;
    case PARSER_LENGTH_LO:
        parser.length = value;
        parser.crc16 = crc16_update(parser.crc16, value);
        parser.state = PARSER_LENGTH_HI;
        break;
    case PARSER_LENGTH_HI:
        parser.length |= (uint16_t)value << 8;
        parser.crc16 = crc16_update(parser.crc16, value);
        parser.index = 0U;
        parser.state = parser.length == 0U ? PARSER_CRC_LO : PARSER_PAYLOAD;
        if (parser.length > HEATGUARD_UPGRADE_MAX_PAYLOAD) {
            parser_reset();
            session_abort();
        }
        break;
    case PARSER_PAYLOAD:
        parser.payload[parser.index++] = value;
        parser.crc16 = crc16_update(parser.crc16, value);
        if (parser.index == parser.length) {
            parser.state = PARSER_CRC_LO;
        }
        break;
    case PARSER_CRC_LO:
        parser.expected_crc16 = value;
        parser.state = PARSER_CRC_HI;
        break;
    case PARSER_CRC_HI:
        parser.expected_crc16 |= (uint16_t)value << 8;
        if (parser.expected_crc16 == parser.crc16) {
            handle_frame();
        } else {
            session_abort();
        }
        parser_reset();
        break;
    default:
        parser_reset();
        break;
    }
}

void upgrade_init(UpgradeReadyCallback ready_callback)
{
    upgrade_ready_callback = ready_callback;
    parser_reset();
    session_abort();
    upgrade_transport_init();
}

void upgrade_task(void *argument)
{
    uint8_t byte;

    (void)argument;
    for (;;) {
        if (upgrade_transport_receive(&byte, portMAX_DELAY) == pdTRUE) {
            if (upgrade_transport_take_overflow() != 0U) {
                session_abort();
                parser_reset();
            }
            parser_feed(byte);
        }
    }
}

void upgrade_commit_after_safe_state(void)
{
    if (session.verified != 0U) {
        boot_handoff_commit_and_reset();
    }
}
