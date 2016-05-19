// This file is a part of the IncludeOS unikernel - www.includeos.org
//
// Copyright 2015-2016 Oslo and Akershus University College of Applied Sciences
// and Alfred Bratterud
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <os>
#include <net/inet4>
#include <sstream>
//#include <http>
#include "server/server.hpp"

std::unique_ptr<server::Server> server_;

#include <memdisk>
#include <fs/fat.hpp> // FAT32 filesystem
using namespace fs;
using namespace std;

////// DISK //////
// instantiate disk with filesystem
auto disk = fs::new_shared_memdisk();

void recursive_fs_dump(vector<fs::Dirent> entries, int depth = 1) {
  auto& filesys = disk->fs();
  int indent = (depth * 3);
  for (auto entry : entries) {

    // Print directories
    if (entry.is_dir()) {
      // Normal dirs
      if (entry.name() != "."  and entry.name() != "..") {
        printf(" %*s-[ %s ]\n", indent, "+", entry.name().c_str());
        recursive_fs_dump(*filesys.ls(entry).entries, depth + 1 );
      } else {
        printf(" %*s  %s \n", indent, "+", entry.name().c_str());
      }

    }else {
      // Print files / symlinks etc.
      //printf(" %*s  \n", indent, "|");
      printf(" %*s-> %s \n", indent, "+", entry.name().c_str());
    }
  }
  printf(" %*s \n", indent, " ");
  //printf(" %*s \n", indent, "o");

}


template <typename PTR>
class BufferWrapper {

  using ptr_t = PTR;

  ptr_t data;
  size_t size;

public:

  BufferWrapper(ptr_t ptr, size_t sz) :
    data {ptr}, size{sz}
  {}

  const ptr_t begin() { return data; }
  const ptr_t end() { return data + size; }
};

#include "middleware/waitress.cpp"

#include "bucket.hpp"
#include "app/squirrel.hpp"

using SquirrelBucket = bucket::Bucket<acorn::Squirrel>;
std::shared_ptr<SquirrelBucket> squirrels;

#include "middleware/parsley.hpp"

void Service::start() {

  uri::URI uri1("asdf");

  printf("<URI> Test URI: %s \n", uri1.to_string().c_str());

  // mount the main partition in the Master Boot Record
  disk->mount([](fs::error_t err) {

      if (err)  panic("Could not mount filesystem, retreating...\n");

      server::Router routes;

      routes.on_get("/api/users/.*", [](auto req, auto res) {
          res->add_header(http::header_fields::Entity::Content_Type,
                          "text/JSON; charset=utf-8"s)
            .add_body("{\"id\" : 1, \"name\" : \"alfred\"}"s);

          res->send();
        });

      routes.on_get("/books/.*", [](auto req, auto res) {
          res->add_header(http::header_fields::Entity::Content_Type,
                          "text/HTML; charset=utf-8"s)
            .add_body("<html><body>"
                      "<h1>Books:</h1>"
                      "<ul>"
                      "<li> borkman.txt </li>"
                      "<li> fables.txt </li>"
                      "<li> poetics.txt </li>"
                      "</ul>"
                      "</body></html>"s
                      );

          res->send();
        });

      routes.on_get("/images/.*", [](auto req, auto res) {
          disk->fs().stat(req->uri().path(), [res](auto err, const auto& entry) {
              if(!err)
                res->send_file({disk, entry});
              else
                res->send_code(http::Not_Found);
        });

      });

      /* Route: GET / */
      routes.on_get(R"(index\.html?|\/|\?)", [](auto, auto res){
          disk->fs().readFile("/index.html", [res] (fs::error_t err, fs::buffer_t buff, size_t len) {
              if(err) {
                res->set_status_code(http::Not_Found);
              } else {
                // fill Response with content from index.html
                printf("<Route#GET:/> Responding with index.html. \n");
                res->add_header(http::header_fields::Entity::Content_Type, "text/html; charset=utf-8"s)
                  .add_body(std::string{(const char*) buff.get(), len});
              }
              res->send();
            });

        }); // << fs().readFile

      routes.on_get("/api/squirrels", [](auto, auto res) {

      });
      // initialize server
      server_ = std::make_unique<server::Server>();
      server_->set_routes(routes).listen(8081);

      /*
      // add a middleware as lambda
      acorn->use([](auto req, auto res, auto next){
        hw::PIT::on_timeout(0.050, [next]{
          printf("<MW:lambda> EleGiggle (50ms delay)\n");
          (*next)();
        });
      });
      */

      // setup database
      squirrels = std::make_shared<SquirrelBucket>();
      squirrels->spawn("Andreas"s, 28U, "Code Monkey"s);
      squirrels->spawn("Alf"s, 5U, "Script kiddie"s);

      printf("JSON: %s\n", stringerj::StringerJ::array("squirrels"s, squirrels->lineup()).c_str());


      // custom middleware to serve static files
      server::Middleware_ptr waitress = std::make_shared<Waitress>(disk);
      server_->use(waitress);

      server::Middleware_ptr parsley = std::make_shared<Parsley>();
      server_->use(parsley);


      auto vec = disk->fs().ls("/").entries;


      printf("------------------------------------ \n");
      printf(" Memdisk contents \n");
      printf("------------------------------------ \n");
      recursive_fs_dump(*vec);
      printf("------------------------------------ \n");

      hw::PIT::instance().onRepeatedTimeout(15s, []{
        printf("%s\n", server_->ip_stack().tcp().status().c_str());
      });

    }); // < disk*/
}
