-- Sample configuration file for the bridge application

-- Bind endpoint. Control socket binds to bind_base_port. Event socket binds to
-- bind_base_port + 1.
bind_address = os.getenv("BRIDGE_BIND_ADDRESS") or "*"
bind_base_port = os.getenv("BRIDGE_BIND_BASE_PORT") or 5555

-- CurveZMQ keys to be used for communication between bridge nodes
-- NOTE! These are test keys (http://api.zeromq.org/4-2:zmq-curve) which by
-- definition are known keys. They cannot be used to authenticate nodes.
-- Set BRIDGE_USE_CURVE environment variable to use CURVE mechanism.
if os.getenv("BRIDGE_USE_CURVE") then
   curve_secret_key = "JTKVSB%%)wK0E.X)V>+}o?pNmC{O&4W4b!Ni{Lh6"
   curve_public_key = "rq:rM>}U?@Lns47E1%kR.o@n%FcmmsL/@{H8]yf7"
end

game {
   uuid = "61b431d1-386b-4b16-a99d-d48dd35f1a4e",
}
