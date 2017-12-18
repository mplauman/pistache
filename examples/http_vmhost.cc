/*
 * Matthew Plaumann, 16 December 2017
 *
 * Example of a VMWare ESXi server listening for guest VM connections.
 */

#include <pistache/endpoint.h>

#define linux
#include <sys/types.h>
#include <vmci/vmci_sockets.h>

using namespace Pistache;

class HelloHandler : public Http::Handler {
public:

    HTTP_PROTOTYPE(HelloHandler)

    void onRequest(const Http::Request& request, Http::ResponseWriter response) {
        response.send(Http::Code::Ok, "Hello World");
    }
};

int main(int argc, char * argv[]) {
    auto opts = Http::Endpoint::options()
        .threads(1);

    std::string port("8080");
    if (argc > 1) {
        port = argv[1];
    }

    Http::Endpoint server([port](const Flags<Tcp::Options> &) -> int {
        struct sockaddr_vm addr = {};
        addr.svm_family = ::VMCISock_GetAFValue();
        addr.svm_cid = VMADDR_CID_ANY;
        addr.svm_port = std::stoi(port);

        int fd = ::socket(addr.svm_family, SOCK_STREAM, 0);

        if (fd >= 0) {
            TRY(::bind(fd, reinterpret_cast<const struct sockaddr*>(&addr), sizeof(addr)));
        }

        return fd;
    });

    server.init(opts);
    server.setHandler(Http::make_handler<HelloHandler>());
    server.serve();

    server.shutdown();
}
