#include "common/repository.hpp"
#include "common/connection.hpp"
#include "common/protocol.hpp"
#include "fmt/printf.h"
#include <cstdio>
#include <thread>
#include <atomic>
#include <unordered_map>
#include "yaml-cpp/yaml.h"
#include "common/log.hpp"

using namespace std;

struct Server{
    bool IsRunning = true;
    RepositoriesRegistry Registry;
    RepositoriesConfig Config;
    TcpListener ConnectionListener;

    std::unordered_map<IpAddress, Connection> Connections;

    Server(const char *config_filepath){
        ConnectionListener.listen(s_DefaultServerPort);
        ConnectionListener.setBlocking(false);

        if(!Config.LoadFromNode(YAML::LoadFile(config_filepath)))
            throw Exception("Can't load config");

        for(auto [name, path]: Config)
            Registry.OpenRepository(std::move(path), std::move(name));
    }

    void CheckPendingConnections(){
        Connection connection;
        if(ConnectionListener.accept(connection) == Socket::Done){
            
            Log("{}:{} connected", connection.getRemoteAddress().toString(), connection.getRemotePort());

            auto remote = connection.getRemoteAddress();


            AllRepositoriesStateNotify info; // XXX excess copying
            info.Repositories.reserve(Registry.Repositories.size());
            for(auto [name, repo]: Registry.Repositories)
                info.Repositories.push_back({std::move(name), std::move(repo.LastState)});

            connection.Send(info);

            Connections.emplace(connection.getRemoteAddress(), std::move(connection));
        }
    }

    void CheckPendingRequests(){
        for(auto &c: Connections){
            
            sf::Packet packet;
            if(c.second.receive(packet) == Socket::Done)
                Log("Connection {}:{} has sent {} bytes\n", c.second.getRemoteAddress().toString(), c.second.getRemotePort(), packet.getDataSize());
        }
    }

    void PollRepositoriesState(){
        for(auto &[name, repo]: Registry.Repositories){
            auto ops = repo.UpdateState();
            if(ops.size())
                PushChanges(name, repo);
        }
    }

    void PushChanges(const std::string &name, const Repository &repo){
        Log("Pushing changes\n");

        for(auto &[addresss, connection]: Connections)
            connection.Send(RepositoryStateNotify{name, repo.LastState});
    }

    void Run(){
        while(IsRunning){
            CheckPendingConnections();
            CheckPendingRequests();

            PollRepositoriesState();

            std::this_thread::sleep_for(1s);
        }
    }
};

int main(){
    Server server("server_config.yaml");

    server.Run();
}