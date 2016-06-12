#include <QCoreApplication>
#include <iostream>

#include "include/imgserver/restclient.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    std::cout << "start" << std::endl;

    RestClient *client = new RestClient();
    client->get("http://localhost:8080/imgserver/rest");

    std::cout << "end" << std::endl;

    return a.exec();
}