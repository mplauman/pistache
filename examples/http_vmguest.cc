/*
 * Matthew Plaumann, 18 December 2017
 *
 * An example HTTP client using VMware's VSocket technology to issue RESTful
 * requests to its ESXi host.
 */

#include <atomic>

#include <pistache/net.h>
#include <pistache/http.h>
#include <pistache/client.h>

#define linux
#include <sys/types.h>
#include <vmci/vmci_sockets.h>

using namespace Pistache;
using namespace Pistache::Http;

class VSocket : public ClientSocket
{
public:
    VSocket(const Address &addr)
        : ClientSocket(::socket(::VMCISock_GetAFValue(), SOCK_STREAM, 0))
        , cid(std::stoi(addr.host()))
        , port(addr.port())
    {
    }

    int connect() override
    {
        struct sockaddr_vm addr = {};
        addr.svm_family = ::VMCISock_GetAFValue();
        addr.svm_cid = cid;
        addr.svm_port = port;

        return ::connect(static_cast<int>(*this), reinterpret_cast<const struct sockaddr *>(&addr), sizeof (addr));
    }

    static
    VSocket *create(const Address& addr)
    {
        return new VSocket(addr);
    }

private:
    int cid;
    int port;
};

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: run_http_vmguest page [count]" << std::endl;
        std::cerr << std::endl;
        std::cerr << "The host component of the page's URL is the CID of the" << std::endl;
        std::cerr << "target machine. This is typically `2` for ESXi hosts." << std::endl;
        std::cerr << std::endl;
        std::cerr << "Example: run_http_vmguest http://2:8080/index.html" << std::endl;
        return 1;
    }

    std::string page = argv[1];
    int count = 1;
    if (argc == 3) {
        count = std::stoi(argv[2]);
    }

    Http::Client client(VSocket::create);

    auto opts = Http::Client::options()
        .threads(1)
        .maxConnectionsPerHost(8);
    client.init(opts);

    std::vector<Async::Promise<Http::Response>> responses;

    std::atomic<size_t> completedRequests(0);
    std::atomic<size_t> failedRequests(0);

    auto start = std::chrono::system_clock::now();

    for (int i = 0; i < count; ++i) {
        auto resp = client.get(page).cookie(Http::Cookie("FOO", "bar")).send();
        resp.then([&](Http::Response response) {
                ++completedRequests;
            std::cout << "Response code = " << response.code() << std::endl;
            auto body = response.body();
            if (!body.empty())
               std::cout << "Response body = " << body << std::endl;
        }, Async::IgnoreException);
        responses.push_back(std::move(resp));
    }

    auto sync = Async::whenAll(responses.begin(), responses.end());
    Async::Barrier<std::vector<Http::Response>> barrier(sync);

    barrier.wait_for(std::chrono::seconds(5));

    auto end = std::chrono::system_clock::now();
    std::cout << "Summary of execution" << std::endl
              << "Total number of requests sent     : " << count << std::endl
              << "Total number of responses received: " << completedRequests.load() << std::endl
              << "Total number of requests failed   : " << failedRequests.load() << std::endl
              << "Total time of execution           : "
              << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms" << std::endl;

    client.shutdown();
}
