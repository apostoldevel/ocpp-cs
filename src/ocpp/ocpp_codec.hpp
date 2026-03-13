#pragma once
//
// OcppCodec — WsCodec implementation for OCPP 1.6 JSON wire format.
//
// Converts between WsMessage and OCPP array format:
//   Call:       [2, uniqueId, action, payload]
//   CallResult: [3, uniqueId, payload]
//   CallError:  [4, uniqueId, errorCode, errorDescription, details]
//

#include "apostol/ws_client.hpp"
#include "ocpp/protocol.hpp"

#include <stdexcept>

namespace ocpp
{

class OcppCodec final : public apostol::WsCodec
{
public:
    std::string serialize(const apostol::WsMessage& msg) const override
    {
        OcppMessage m;
        m.unique_id = msg.id;

        switch (msg.type) {
        case apostol::WsMessage::Type::Request:
            m.type    = MessageType::Call;
            m.action  = msg.action;
            m.payload = msg.payload;
            break;
        case apostol::WsMessage::Type::Response:
            m.type    = MessageType::CallResult;
            m.payload = msg.payload;
            break;
        case apostol::WsMessage::Type::Error:
            m.type              = MessageType::CallError;
            m.error_code        = msg.error_code;
            m.error_description = msg.error_description;
            m.payload           = msg.payload.is_null()
                                    ? nlohmann::json::object() : msg.payload;
            break;
        }

        return serialize_ocpp_json(m);
    }

    apostol::WsMessage deserialize(std::string_view text) const override
    {
        auto m = parse_ocpp_json(text);

        apostol::WsMessage msg;
        msg.id = std::move(m.unique_id);

        switch (m.type) {
        case MessageType::Call:
            msg.type    = apostol::WsMessage::Type::Request;
            msg.action  = std::move(m.action);
            msg.payload = std::move(m.payload);
            break;
        case MessageType::CallResult:
            msg.type    = apostol::WsMessage::Type::Response;
            msg.payload = std::move(m.payload);
            break;
        case MessageType::CallError:
            msg.type              = apostol::WsMessage::Type::Error;
            msg.error_code        = std::move(m.error_code);
            msg.error_description = std::move(m.error_description);
            msg.payload           = std::move(m.payload);
            break;
        }

        return msg;
    }
};

} // namespace ocpp
