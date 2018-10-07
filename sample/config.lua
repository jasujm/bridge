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

-- With UUID only the game is peerless (clients only) and the backend controls
-- all positions. Uncomment necessary keyword arguments to configure game with
-- peers.

game {
   uuid = "61b431d1-386b-4b16-a99d-d48dd35f1a4e",

   -- For a game with peers you need to configure which positions this peer
   -- controls. The bridge backend application does not autonegotiate positions
   -- so everything needs to be agreed in advance.

   -- positions_controlled = { "north" }

   -- List peers to connect to. Ensure that the other peers are configured to
   -- cover all positions and only one position is controlled by one peer. This
   -- also sets all peers to have the same curve server key as this node. It's
   -- not necessary.

   -- peers = {
   --    { endpoint="tcp://peer1.example.com", server_key = curve_public_key },
   --    { endpoint="tcp://peer2.example.com", server_key = curve_public_key },
   --    { endpoint="tcp://peer3.example.com", server_key = curve_public_key },
   -- },

   -- To configure card exchange using card server, uncomment the following
   -- section. Bridge backend does not start the card server process, so it must
   -- be started separately with the same control and base peer endpoints
   -- defined here. Peer endpoints need to be accessible by peers. For control
   -- endpoint it's not necessary or even advisable.

   -- card_server = os.getenv("BRIDGE_USE_CS") and {
   --    control_endpoint = "tcp://127.0.0.1:5560",
   --    base_peer_endpoint = "tcp://*:5565",
   -- },
}
