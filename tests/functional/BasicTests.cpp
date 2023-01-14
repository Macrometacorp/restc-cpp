
#include <iostream>

#include "restc-cpp/logging.h"

#include <boost/lexical_cast.hpp>
#include <boost/fusion/adapted.hpp>

#include "restc-cpp/restc-cpp.h"
#include "restc-cpp/RequestBuilder.h"

#include "gtest/gtest.h"
#include "restc-cpp/test_helper.h"


using namespace std;
using namespace restc_cpp;


// For entries received from http://jsonplaceholder.typicode.com/posts
struct Post {
    int id = 0;
    string username;
    string motto;
};

BOOST_FUSION_ADAPT_STRUCT(
    Post,
    (int, id)
    (string, username)
    (string, motto)
)

const string http_url = "http://localhost:3001/normal/manyposts";
const string https_url = "https://lastviking.eu/files/api";


//void DoSomethingInteresting(Context& ctx) {


//    // Asynchronously fetch the entire data-set, and convert it from json
//    // to C++ objects was we go.
//    // We expcet a list of Post objects
//    list<Post> posts_list;
//    SerializeFromJson(posts_list, ctx.Get(GetDockerUrl(http_url)));

//    // Just dump the data.
//    for(const auto& post : posts_list) {
//        RESTC_CPP_LOG_INFO_("Post id=" << post.id << ", title: " << post.motto);
//    }

//    // Asynchronously connect to server and POST data.
//    auto repl = ctx.Post(GetDockerUrl(http_url), "{\"test\":\"teste\"}");

//    // Asynchronously fetch the entire data-set and return it as a string.
//    auto json = repl->GetBodyAsString();
//    RESTC_CPP_LOG_INFO_("Received POST data: " << json);


//    // Use RequestBuilder to fetch everything
//    repl = RequestBuilder(ctx)
//        .Get(GetDockerUrl(http_url))
//        .Header("X-Client", "RESTC_CPP")
//        .Header("X-Client-Purpose", "Testing")
//        .Header("Accept", "*/*")
//        .Execute();

//    string body = repl->GetBodyAsString();
//    cout << "Got compressed list: " << body << endl;
//    repl.reset();

//    // Use RequestBuilder to fetch a record
//    repl = RequestBuilder(ctx)
//        .Get(GetDockerUrl(http_url))
//        .Header("X-Client", "RESTC_CPP")
//        .Header("X-Client-Purpose", "Testing")
//        .Header("Accept", "*/*")
//        .Argument("id", 1)
//        .Argument("test some $ stuff", "oh my my")
//        .Execute();

//    cout << "Got: " << repl->GetBodyAsString() << endl;
//    repl.reset();

//    // Use RequestBuilder to fetch a record without compression
//    repl = RequestBuilder(ctx)
//        .Get(GetDockerUrl(http_url))
//        .Header("X-Client", "RESTC_CPP")
//        .Header("X-Client-Purpose", "Testing")
//        .Header("Accept", "*/*")
//        .DisableCompression()
//        .Argument("id", 2)
//        .Execute();

//    cout << "Got: " << repl->GetBodyAsString() << endl;
//    repl.reset();

//    // Use RequestBuilder to post a record
//    Post data_object;
//    data_object.username = "testid";
//    data_object.motto = "Carpe diem";
//    repl = RequestBuilder(ctx)
//        .Post(GetDockerUrl(http_url))
//        .Header("X-Client", "RESTC_CPP")
//        .Data(data_object)
//        .Execute();

//    repl.reset();

//#ifdef RESTC_CPP_WITH_TLS
//        // Try with https
//        repl = ctx.Get(https_url);
//        json = repl->GetBodyAsString();
//        RESTC_CPP_LOG_INFO_("Received https GET data: " << json);
//#endif // TLS
//        RESTC_CPP_LOG_INFO_("Done");
//}



//    try {
//        auto rest_client = RestClient::Create();
//        auto future = rest_client->ProcessWithPromise(DoSomethingInteresting);

//        // Hold the main thread to allow the worker to do it's job
//        future.get();
//    } catch (const exception& ex) {
//        RESTC_CPP_LOG_INFO_("main: Caught exception: " << ex.what());
//    }

//    // Fetch a result trough a future
//    try {
//        auto client = RestClient::Create();
//        Post my_post = client->ProcessWithPromiseT<Post>([&](Context& ctx) {
//            Post post;
//            SerializeFromJson(post, ctx.Get(GetDockerUrl(http_url) + "/1"));
//            return post;
//        }).get();

//        cout << "Received post# " << my_post.id << ", username: " << my_post.username;
//    } catch (const exception& ex) {
//        RESTC_CPP_LOG_INFO_("main: Caught exception: " << ex.what());
//    }

//    return 0;
//}

TEST(testGeneral, validateGtest) {
    EXPECT_EQ(1, 1);
}

//TEST(ExampleWorkflow, all) {
//    auto cb = [](Context& ctx) -> void {
//        // Asynchronously fetch the entire data-set, and convert it from json
//        // to C++ objects was we go.
//        // We expcet a list of Post objects
//        list<Post> posts_list;
//        SerializeFromJson(posts_list, ctx.Get(GetDockerUrl(http_url)));

//        EXPECT_GE(posts_list.size(), 1);

//        // Asynchronously connect to server and POST data.
//        auto repl = ctx.Post(GetDockerUrl(http_url), "{\"test\":\"teste\"}");

//        // Asynchronously fetch the entire data-set and return it as a string.
//        auto json = repl->GetBodyAsString();
//        RESTC_CPP_LOG_INFO_("Received POST data: " << json);
//        EXPECT_EQ(repl->GetHttpResponse().status_code, 200);

//        // Use RequestBuilder to fetch everything
//        repl = RequestBuilder(ctx)
//            .Get(GetDockerUrl(http_url))
//            .Header("X-Client", "RESTC_CPP")
//            .Header("X-Client-Purpose", "Testing")
//            .Header("Accept", "*/*")
//            .Execute();

//        string body = repl->GetBodyAsString();
//        cout << "Got compressed list: " << body << endl;

//        EXPECT_EQ(repl->GetHttpResponse().status_code, 200);
//        EXPECT_FALSE(body.empty());

//        repl.reset();

//        // Use RequestBuilder to fetch a record
//        repl = RequestBuilder(ctx)
//            .Get(GetDockerUrl(http_url))
//            .Header("X-Client", "RESTC_CPP")
//            .Header("X-Client-Purpose", "Testing")
//            .Header("Accept", "*/*")
//            .Argument("id", 1)
//            .Argument("test some $ stuff", "oh my my")
//            .Execute();

//        EXPECT_EQ(repl->GetHttpResponse().status_code, 200);
//        EXPECT_FALSE(body.empty());
//        cout << "Got: " << repl->GetBodyAsString() << endl;
//        repl.reset();

//        // Use RequestBuilder to fetch a record without compression
//        repl = RequestBuilder(ctx)
//            .Get(GetDockerUrl(http_url))
//            .Header("X-Client", "RESTC_CPP")
//            .Header("X-Client-Purpose", "Testing")
//            .Header("Accept", "*/*")
//            .DisableCompression()
//            .Argument("id", 2)
//            .Execute();

//        cout << "Got: " << repl->GetBodyAsString() << endl;
//        repl.reset();

//        // Use RequestBuilder to post a record
//        Post data_object;
//        data_object.username = "testid";
//        data_object.motto = "Carpe diem";
//        repl = RequestBuilder(ctx)
//            .Post(GetDockerUrl(http_url))
//            .Header("X-Client", "RESTC_CPP")
//            .Data(data_object)
//            .Execute();

//        EXPECT_EQ(repl->GetHttpResponse().status_code, 200);
//        repl.reset();

//#ifdef RESTC_CPP_WITH_TLS
//            // Try with https
//            repl = ctx.Get(https_url);
//            json = repl->GetBodyAsString();
//            EXPECT_EQ(repl->GetHttpResponse().status_code, 200);
//            EXPECT_FALSE(body.empty());
//            RESTC_CPP_LOG_INFO_("Received https GET data: " << json);
//#endif // TLS
//            RESTC_CPP_LOG_INFO_("Done");
//    };

//    auto rest_client = RestClient::Create();
//    auto future = rest_client->ProcessWithPromise(cb);

//    // Hold the main thread to allow the worker to do it's job
//    future.get();
//}

TEST(Request, HttpGetOk) {
    auto client = RestClient::Create();
    EXPECT_TRUE(client);

    client->Process([](Context& ctx) {
        EXPECT_NO_THROW(
            auto repl = ctx.Get(GetDockerUrl(http_url));
            EXPECT_TRUE(repl);
            if (repl) {
                auto body = repl->GetBodyAsString();
                EXPECT_EQ(repl->GetHttpResponse().status_code, 200);
                EXPECT_FALSE(body.empty());
            }
        ); // EXPECT_NO_THROW
    });
}

int main(int argc, char *argv[]) {

    RESTC_CPP_TEST_LOGGING_SETUP("trace");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
