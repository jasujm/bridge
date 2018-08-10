-- Sample configuration file for the bridge application

-- Bind endpoint. Control socket binds to bind_base_port. Event socket binds to
-- bind_base_port + 1.
bind_address = "*"
bind_base_port = 5555

-- CurveZMQ keys to be used for communication between bridge nodes
-- NOTE! These are test keys (http://api.zeromq.org/4-2:zmq-curve) which by
-- definition are known keys. They cannot be used to authenticate nodes.
curve_secret_key = "JTKVSB%%)wK0E.X)V>+}o?pNmC{O&4W4b!Ni{Lh6"
curve_public_key = "rq:rM>}U?@Lns47E1%kR.o@n%FcmmsL/@{H8]yf7"
