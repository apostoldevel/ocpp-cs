#include <catch2/catch_test_macros.hpp>
#include "ocpp/protocol.hpp"
#include "ocpp/ocpp_codec.hpp"

using namespace ocpp;

// ── parse_ocpp_json ─────────────────────────────────────────────────────────

TEST_CASE("parse_ocpp_json: Call message", "[ocpp][protocol]")
{
    auto msg = parse_ocpp_json(
        R"([2,"abc123","BootNotification",{"chargePointVendor":"Test","chargePointModel":"T1"}])");
    REQUIRE(msg.type == MessageType::Call);
    REQUIRE(msg.unique_id == "abc123");
    REQUIRE(msg.action == "BootNotification");
    REQUIRE(msg.payload["chargePointVendor"] == "Test");
}

TEST_CASE("parse_ocpp_json: CallResult message", "[ocpp][protocol]")
{
    auto msg = parse_ocpp_json(R"([3,"abc123",{"status":"Accepted","interval":600}])");
    REQUIRE(msg.type == MessageType::CallResult);
    REQUIRE(msg.unique_id == "abc123");
    REQUIRE(msg.action.empty());
    REQUIRE(msg.payload["status"] == "Accepted");
    REQUIRE(msg.payload["interval"] == 600);
}

TEST_CASE("parse_ocpp_json: CallError message", "[ocpp][protocol]")
{
    auto msg = parse_ocpp_json(
        R"([4,"abc123","InternalError","Something went wrong",{}])");
    REQUIRE(msg.type == MessageType::CallError);
    REQUIRE(msg.unique_id == "abc123");
    REQUIRE(msg.error_code == "InternalError");
    REQUIRE(msg.error_description == "Something went wrong");
}

TEST_CASE("parse_ocpp_json: invalid message type throws", "[ocpp][protocol]")
{
    REQUIRE_THROWS_AS(parse_ocpp_json(R"([9,"id","Action",{}])"), std::invalid_argument);
}

TEST_CASE("parse_ocpp_json: malformed JSON throws", "[ocpp][protocol]")
{
    REQUIRE_THROWS(parse_ocpp_json("not json at all"));
}

// ── serialize_ocpp_json ─────────────────────────────────────────────────────

TEST_CASE("serialize_ocpp_json: Call roundtrip", "[ocpp][protocol]")
{
    OcppMessage msg;
    msg.type = MessageType::Call;
    msg.unique_id = "test1";
    msg.action = "Heartbeat";
    msg.payload = nlohmann::json::object();

    auto json_str = serialize_ocpp_json(msg);
    auto parsed = nlohmann::json::parse(json_str);

    REQUIRE(parsed.is_array());
    REQUIRE(parsed[0] == 2);
    REQUIRE(parsed[1] == "test1");
    REQUIRE(parsed[2] == "Heartbeat");
    REQUIRE(parsed[3].is_object());
}

TEST_CASE("serialize_ocpp_json: CallResult", "[ocpp][protocol]")
{
    OcppMessage msg;
    msg.type = MessageType::CallResult;
    msg.unique_id = "r1";
    msg.payload = {{"status", "Accepted"}};

    auto json_str = serialize_ocpp_json(msg);
    auto parsed = nlohmann::json::parse(json_str);

    REQUIRE(parsed[0] == 3);
    REQUIRE(parsed[1] == "r1");
    REQUIRE(parsed[2]["status"] == "Accepted");
}

TEST_CASE("serialize_ocpp_json: CallError", "[ocpp][protocol]")
{
    OcppMessage msg;
    msg.type = MessageType::CallError;
    msg.unique_id = "e1";
    msg.error_code = "GenericError";
    msg.error_description = "test error";
    msg.payload = nlohmann::json::object();

    auto json_str = serialize_ocpp_json(msg);
    auto parsed = nlohmann::json::parse(json_str);

    REQUIRE(parsed[0] == 4);
    REQUIRE(parsed[1] == "e1");
    REQUIRE(parsed[2] == "GenericError");
    REQUIRE(parsed[3] == "test error");
    REQUIRE(parsed[4].is_object());
}

// ── Full roundtrip ──────────────────────────────────────────────────────────

TEST_CASE("parse/serialize: Call roundtrip", "[ocpp][protocol]")
{
    auto original = R"([2,"u1","Reset",{"type":"Hard"}])";
    auto msg = parse_ocpp_json(original);
    auto serialized = serialize_ocpp_json(msg);
    auto reparsed = parse_ocpp_json(serialized);

    REQUIRE(reparsed.type == msg.type);
    REQUIRE(reparsed.unique_id == msg.unique_id);
    REQUIRE(reparsed.action == msg.action);
    REQUIRE(reparsed.payload == msg.payload);
}

// ── generate_unique_id ──────────────────────────────────────────────────────

TEST_CASE("generate_unique_id: returns non-empty unique strings", "[ocpp][protocol]")
{
    auto id1 = generate_unique_id();
    auto id2 = generate_unique_id();
    REQUIRE(!id1.empty());
    REQUIRE(!id2.empty());
    REQUIRE(id1 != id2);
}

// ── make_call / make_call_result / make_call_error ──────────────────────────

TEST_CASE("make_call: creates correct OcppMessage", "[ocpp][protocol]")
{
    auto msg = make_call("Heartbeat", nlohmann::json::object());
    REQUIRE(msg.type == MessageType::Call);
    REQUIRE(msg.action == "Heartbeat");
    REQUIRE(!msg.unique_id.empty());
}

TEST_CASE("make_call_result: creates correct OcppMessage", "[ocpp][protocol]")
{
    auto msg = make_call_result("req1", {{"status", "Accepted"}});
    REQUIRE(msg.type == MessageType::CallResult);
    REQUIRE(msg.unique_id == "req1");
    REQUIRE(msg.payload["status"] == "Accepted");
}

TEST_CASE("make_call_error: creates correct OcppMessage", "[ocpp][protocol]")
{
    auto msg = make_call_error("req1", "NotImplemented", "Not supported");
    REQUIRE(msg.type == MessageType::CallError);
    REQUIRE(msg.unique_id == "req1");
    REQUIRE(msg.error_code == "NotImplemented");
    REQUIRE(msg.error_description == "Not supported");
}

// ── OCPP enums ──────────────────────────────────────────────────────────────

TEST_CASE("ChargePointStatus: string conversion roundtrip", "[ocpp][protocol]")
{
    REQUIRE(charge_point_status_to_string(ChargePointStatus::Available) == "Available");
    REQUIRE(charge_point_status_to_string(ChargePointStatus::Charging) == "Charging");
    REQUIRE(charge_point_status_to_string(ChargePointStatus::Faulted) == "Faulted");

    REQUIRE(string_to_charge_point_status("Available") == ChargePointStatus::Available);
    REQUIRE(string_to_charge_point_status("Charging") == ChargePointStatus::Charging);
    REQUIRE(string_to_charge_point_status("Faulted") == ChargePointStatus::Faulted);
}

TEST_CASE("RegistrationStatus: string conversion", "[ocpp][protocol]")
{
    REQUIRE(registration_status_to_string(RegistrationStatus::Accepted) == "Accepted");
    REQUIRE(registration_status_to_string(RegistrationStatus::Pending) == "Pending");
    REQUIRE(registration_status_to_string(RegistrationStatus::Rejected) == "Rejected");
}

// ── OcppCodec ──────────────────────────────────────────────────────────────

TEST_CASE("OcppCodec: serialize Request produces Call array", "[ocpp][codec]")
{
    OcppCodec codec;
    apostol::WsMessage msg;
    msg.type    = apostol::WsMessage::Type::Request;
    msg.id      = "req1";
    msg.action  = "Heartbeat";
    msg.payload = nlohmann::json::object();

    auto text = codec.serialize(msg);
    auto arr  = nlohmann::json::parse(text);

    REQUIRE(arr.is_array());
    REQUIRE(arr[0] == 2);
    REQUIRE(arr[1] == "req1");
    REQUIRE(arr[2] == "Heartbeat");
    REQUIRE(arr[3].is_object());
}

TEST_CASE("OcppCodec: serialize Response produces CallResult array", "[ocpp][codec]")
{
    OcppCodec codec;
    apostol::WsMessage msg;
    msg.type    = apostol::WsMessage::Type::Response;
    msg.id      = "res1";
    msg.payload = {{"status", "Accepted"}};

    auto text = codec.serialize(msg);
    auto arr  = nlohmann::json::parse(text);

    REQUIRE(arr[0] == 3);
    REQUIRE(arr[1] == "res1");
    REQUIRE(arr[2]["status"] == "Accepted");
}

TEST_CASE("OcppCodec: serialize Error produces CallError array", "[ocpp][codec]")
{
    OcppCodec codec;
    apostol::WsMessage msg;
    msg.type              = apostol::WsMessage::Type::Error;
    msg.id                = "err1";
    msg.error_code        = "InternalError";
    msg.error_description = "Something failed";

    auto text = codec.serialize(msg);
    auto arr  = nlohmann::json::parse(text);

    REQUIRE(arr[0] == 4);
    REQUIRE(arr[1] == "err1");
    REQUIRE(arr[2] == "InternalError");
    REQUIRE(arr[3] == "Something failed");
    REQUIRE(arr[4].is_object());
}

TEST_CASE("OcppCodec: deserialize Call array to Request", "[ocpp][codec]")
{
    OcppCodec codec;
    auto msg = codec.deserialize(R"([2,"u1","BootNotification",{"vendor":"Test"}])");

    REQUIRE(msg.type == apostol::WsMessage::Type::Request);
    REQUIRE(msg.id == "u1");
    REQUIRE(msg.action == "BootNotification");
    REQUIRE(msg.payload["vendor"] == "Test");
}

TEST_CASE("OcppCodec: deserialize CallResult to Response", "[ocpp][codec]")
{
    OcppCodec codec;
    auto msg = codec.deserialize(R"([3,"u1",{"status":"Accepted","interval":300}])");

    REQUIRE(msg.type == apostol::WsMessage::Type::Response);
    REQUIRE(msg.id == "u1");
    REQUIRE(msg.action.empty());
    REQUIRE(msg.payload["interval"] == 300);
}

TEST_CASE("OcppCodec: deserialize CallError to Error", "[ocpp][codec]")
{
    OcppCodec codec;
    auto msg = codec.deserialize(R"([4,"u1","GenericError","test err",{}])");

    REQUIRE(msg.type == apostol::WsMessage::Type::Error);
    REQUIRE(msg.id == "u1");
    REQUIRE(msg.error_code == "GenericError");
    REQUIRE(msg.error_description == "test err");
}

TEST_CASE("OcppCodec: full roundtrip Request", "[ocpp][codec]")
{
    OcppCodec codec;
    apostol::WsMessage original;
    original.type    = apostol::WsMessage::Type::Request;
    original.id      = "rt1";
    original.action  = "Reset";
    original.payload = {{"type", "Hard"}};

    auto text     = codec.serialize(original);
    auto restored = codec.deserialize(text);

    REQUIRE(restored.type == original.type);
    REQUIRE(restored.id == original.id);
    REQUIRE(restored.action == original.action);
    REQUIRE(restored.payload == original.payload);
}
