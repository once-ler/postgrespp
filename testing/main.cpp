/*
  Copyright (c) 2014, Tolga HOŞGÖR
  All rights reserved.

  This file is part of postgrespp.

  postgrespp is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  postgrespp is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with postgrespp.  If not, see <http://www.gnu.org/licenses/>
*/

#include "include/connection.hpp"
#include "include/utility.hpp"

using namespace postgrespp;

// Reference: https://www.postgresql.org/docs/9.6/static/libpq-exec.html
int main2(int argc, char* argv[]) {
  auto connection = Connection::create("host=127.0.0.1 port=5432 dbname=pccrms connect_timeout=10");

  auto onQueryExecuted = [connection](const boost::system::error_code& ec, Result result) {
    if (!ec) {
      while (result.next()) {
        char* str;
        
        str = result.get<char*>(0);
        std::cout << "STR0: " << str << std::endl;
        std::cout << result.getColumn<char*>(0) << std::endl;
        str = result.get<char*>(1);
        std::cout << "STR1: " << str << std::endl;
        str = result.get<char*>(2);
        std::cout << "STR2: " << str << std::endl;
        str = result.get<char*>(3);
        std::cout << "STR3: " << str << std::endl;
      }
    } else {
      std::cout << ec << std::endl;
    }

    // we are done, stop the service
    Connection::ioService().service().stop();
  };

  connection->queryParams("select * from master.droid", std::move(onQueryExecuted),  Connection::ResultFormat::TEXT);
  
  Connection::ioService().thread().join();

  return 0;
}

/*
* testlibpq2.c
*      Test of the asynchronous notification interface
*
* Start this program, then from psql in another window do
*   NOTIFY TBL2;
* Repeat four times to get this program to exit.
*
* Or, if you want to get fancy, try this:
* populate a database with the following commands
* (provided in src/test/examples/testlibpq2.sql):
*
*   CREATE TABLE TBL1 (i int4);
*
*   CREATE TABLE TBL2 (i int4);
*
*   CREATE RULE r1 AS ON INSERT TO TBL1 DO
*     (INSERT INTO TBL2 VALUES (new.i); NOTIFY TBL2);
*
* and do this four times:
*
*   INSERT INTO TBL1 VALUES (10);
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
// #include <sys/time.h>
#include <libpq-fe.h>

#if defined(_MSC_VER) || defined(_MSC_EXTENSIONS)
#define DELTA_EPOCH_IN_MICROSECS  116444736000000000Ui64 // CORRECT
#else
#define DELTA_EPOCH_IN_MICROSECS  116444736000000000ULL // CORRECT
#endif

static void
exit_nicely(PGconn *conn) {
  PQfinish(conn);
  exit(1);
}

struct PGNotifyResult {
  string channel;
  string data;
  int pid;
};

using Callback = std::function<void(PGNotifyResult)>;

class ClientHandler {
public:
  void operator()(const string& channel, Callback callback = [](const PGNotifyResult& result) {}) {
    const char *conninfo = "dbname = pccrms";
    PGconn     *conn;
    PGresult   *res;
    PGnotify   *notify;
    int         nnotifies;

    /* Make a connection to the database */
    conn = PQconnectdb(conninfo);

    /* Check to see that the backend connection was successfully made */
    if (PQstatus(conn) != CONNECTION_OK) {
      fprintf(stderr, "Connection to database failed: %s",
        PQerrorMessage(conn));
      exit_nicely(conn);
    }

    /*
    * Issue LISTEN command to enable notifications from the rule's NOTIFY.
    */
    string cmd = "LISTEN " + channel;
    res = PQexec(conn, cmd.c_str());
    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
      fprintf(stderr, "LISTEN command failed: %s", PQerrorMessage(conn));
      PQclear(res);
      exit_nicely(conn);
    }

    /*
    * should PQclear PGresult whenever it is no longer needed to avoid memory
    * leaks
    */
    PQclear(res);

    /* Quit after four notifies are received. */
    nnotifies = 0;
    // while (nnotifies < 4) {
    while (true) {
      /*
      * Sleep until something happens on the connection.  We use select(2)
      * to wait for input, but you could also use poll() or similar
      * facilities.
      */
      int         sock;
      fd_set      input_mask;

      sock = PQsocket(conn);

      if (sock < 0)
        break;              /* shouldn't happen */

      FD_ZERO(&input_mask);
      FD_SET(sock, &input_mask);

      if (select(sock + 1, &input_mask, NULL, NULL, NULL) < 0) {
        fprintf(stderr, "select() failed: %s\n", strerror(errno));
        exit_nicely(conn);
      }

      /* Now check for input */
      PQconsumeInput(conn);
      while ((notify = PQnotifies(conn)) != NULL) {
        fprintf(stderr,
          "ASYNC NOTIFY of '%s' received %s from backend PID %d\n",
          notify->relname, notify->extra, notify->be_pid);

        PGNotifyResult result{ notify->relname, notify->extra, notify->be_pid };
        callback(result);

        PQfreemem(notify);
        nnotifies++;
      }
    }

    fprintf(stderr, "Done.\n");

    /* close the connection to the database and cleanup */
    PQfinish(conn);
  }
};

template<typename F>
class BackgroundWorker {
  vector<unique_ptr<thread>> threads;
  F func;
public:
  explicit BackgroundWorker(F func_) : func(func_) {};
  
  template<typename ...U>
  void startMany(U... args) {
    auto params = { args... };
    
    for (const auto& e : params) {
      threads.emplace_back(new thread(func, e));
    }
  }

  template<typename FUNC, typename ...U>
  void start(FUNC callback, U... args) {
    threads.emplace_back(new thread(func, args..., callback));
  }

  /*
  Calling start() is only necessary if calling program does not join on thread.
  */
  void join() {
    auto len = threads.size();
    if (len > 0) {
      threads.at(len - 1)->join();
    }
  }
};

int main(int argc, char* argv[]) {
  {
    auto bg = BackgroundWorker<ClientHandler>(ClientHandler());
    bg.startMany("TBL2", "FOO");
    bg.start([](const PGNotifyResult& result) { cout << result.data << endl; }, "BAR");
    bg.join();
  }

  std::vector<std::unique_ptr<std::thread>> threads;

  threads.emplace_back(new std::thread((ClientHandler()), "TBL2"));
  threads.emplace_back(new std::thread((ClientHandler()), "FOO"));

  for (const auto& t : threads) {
    // t->join();
  }

  // Join the last thread to block.
  threads.at(1)->join();

  return 0;
}