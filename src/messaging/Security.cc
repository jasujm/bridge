#include "messaging/Security.hh"

namespace Bridge {
namespace Messaging {

void setupCurveServer(zmq::socket_t& socket)
{
    socket.setsockopt(ZMQ_CURVE_SERVER, 1);
    socket.setsockopt(ZMQ_CURVE_SECRETKEY, "JTKVSB%%)wK0E.X)V>+}o?pNmC{O&4W4b!Ni{Lh6", 41);
}

void setupCurveClient(zmq::socket_t& socket)
{
    socket.setsockopt(ZMQ_CURVE_SERVER, 0);
    socket.setsockopt(ZMQ_CURVE_SERVERKEY, "rq:rM>}U?@Lns47E1%kR.o@n%FcmmsL/@{H8]yf7", 41);
    socket.setsockopt(ZMQ_CURVE_PUBLICKEY, "Yne@$w-vo<fVvi]a<NY6T1ed:M$fCG*[IaLV{hID", 41);
    socket.setsockopt(ZMQ_CURVE_SECRETKEY, "D:)Q[IlAW!ahhC2ac:9*A}h:p?([4%wOTJ%JR%cs", 41);
}

}
}
