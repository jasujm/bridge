-- Sample configuration file for the bridge application

-- The bind interface and port. Control socket binds to
-- bind_base_port. Event socket binds to bind_base_port + 1.

bind_address = os.getenv("BRIDGE_BIND_ADDRESS") or "*"
bind_base_port = os.getenv("BRIDGE_BIND_BASE_PORT") or 5555

-- Set curve_secret_key and curve_public_key to enable encrypting
-- traffic with CurveZMQ. The keys here are the test keys test keys
-- (http://api.zeromq.org/4-2:zmq-curve) and should be substituted for
-- a keypair you generated.

if os.getenv("BRIDGE_USE_CURVE") then
   curve_secret_key = "JTKVSB%%)wK0E.X)V>+}o?pNmC{O&4W4b!Ni{Lh6"
   curve_public_key = "rq:rM>}U?@Lns47E1%kR.o@n%FcmmsL/@{H8]yf7"
end

-- Set up game(s) using the game function. The simple setup if for
-- peerless games, in which case it's enough to provide uuid to
-- identify the game. To setup a game with peers, you additionally
-- need to specify positions_controlled, peers and, optionally,
-- card_server.

-- Note that peerless games can also be created by the frontend
-- application after startup.

game {
   uuid = "61b431d1-386b-4b16-a99d-d48dd35f1a4e",

   -- For a game with peers you need to configure which positions this
   -- peer controls. The positions need to be agreed in advance, and
   -- together the peers need to control all positions. That is: each
   -- player needs to uncomment exactly one of the lines below.

   -- positions_controlled = { "north" },
   -- positions_controlled = { "east" },
   -- positions_controlled = { "west" },
   -- positions_controlled = { "south" },

   -- Tell the backend where it can finds its peers. Assuming all the
   -- other peers have the same CURVE keypair as yourself (needs not
   -- be the case), you can just assign server_key from the
   -- curve_public_key you defined above.

   -- peers = {
   --    { endpoint="tcp://peer1.example.com:5555", server_key = curve_public_key },
   --    { endpoint="tcp://peer2.example.com:5555", server_key = curve_public_key },
   --    { endpoint="tcp://peer3.example.com:5555", server_key = curve_public_key },
   -- },

   -- To configure card exchange using card server, uncomment the
   -- following section. control_endpoint and base_peer_endpoint are
   -- what you use as command line arguments when running the bridgecs
   -- command. Peer endpoints need to be accessible by the card server
   -- peers. control_endpoint should only be accessible by the bridge
   -- backend.

   -- card_server = os.getenv("BRIDGE_USE_CS") and {
   --    control_endpoint = "tcp://127.0.0.1:5560",
   --    base_peer_endpoint = "tcp://*:5565",
   -- },
}
