#include "common/repository.hpp"
#include "common/connection.hpp"
#include "common/protocol.hpp"
#include <iostream>
#include <cstdio>
#include <thread>
#include <atomic>
#include <unordered_map>

using namespace std;

struct Server{
    bool IsRunning = true;
    RepositoriesRegistry Registry;
    TcpListener ConnectionListener;

    std::unordered_map<IpAddress, Connection> Connections;

    Server(){
        ConnectionListener.listen(s_DefaultServerPort);
        ConnectionListener.setBlocking(false);
    }

    void CheckPendingConnections(){
        Connection connection;
        if(ConnectionListener.accept(connection) == Socket::Done){

            std::cout << "[Connected]: " << connection.getRemoteAddress() << ":" << connection.getRemotePort() << std::endl;

            auto remote = connection.getRemoteAddress();

            SendRepositoriesInfo(connection, Registry.Repositories);

            Connections.emplace(connection.getRemoteAddress(), std::move(connection));
        }
    }

    static void SendRepositoriesInfo(Connection &connection, const std::vector<Repository> &repos){
        Packet packet;

        Header header;
        header.MagicWord = s_MagicWord;
        header.Type = MsgType::RepositoriesInfo;

        packet << header;

        RepositoriesInfo info; // XXX excess copying
        info.Names.reserve(repos.size());
        for(const auto &repo: repos)
            info.Names.push_back(repo.Name);
        
        packet << info;

        connection.send(packet);
    }

    void CheckPendingRequests(){
        for(auto &c: Connections){
            
            sf::Packet packet;
            if(c.second.receive(packet) == Socket::Done)
                std::cout << "Connection: "  << c.second.getRemoteAddress() << ":" << c.second.getRemotePort() << " has sent " << packet.getDataSize() << " bytes\n";
        }
    }

    void PollRepositoriesState(){
        for(auto &repo: Registry.Repositories){
            auto ops = repo.UpdateState();
            if(ops.size())
                PushChanges(repo);
        }
    }

    static void SendRepositoryState(Connection &connection, const Repository &repo){
        Packet packet;

        Header header;
        header.MagicWord = s_MagicWord;
        header.Type = MsgType::RepositoryStateNotify;

        packet << header;

        RepositoryStateNotify notify;//XXX excessive copy// we can serialize Repository instead of this shit
        notify.Name = repo.Name;
        notify.State = repo.LastState; 

        packet << notify;

        connection.send(packet);
    }

    void PushChanges(const Repository &repo){
        std::cout << "Pushing Changes\n";

        for(auto &c: Connections)
            SendRepositoryState(c.second, repo);
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
    fs::path path = "/home/hephaestus/Dev/Cortex/RunDir";

    Server server;
    server.Registry.OpenRepository("/home/hephaestus/Dev/Cortex/RunDir/TestRepoStorage", "TestRepoName");

    server.Run();
}