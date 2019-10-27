#include <pistache/http.h>
#include <pistache/router.h>
#include <pistache/endpoint.h>
#include <iostream>

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>

#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>
#include <opencv2/core/mat.hpp>
#include <opencv2/core/cvstd.hpp>

#include "arcsoft_face_sdk.h"
#include "amcomdef.h"
#include "asvloffscreen.h"
#include "merror.h"
#include <iostream>
#include <stdarg.h>
#include <string>
#include <sstream>
#include "shared_queue.hpp"
#pragma comment(lib, "libarcsoft_face_engine.lib")
#pragma comment(lib, "libpistache.lib")

#define psw1 "txwUMMgDJr1SZurx1HV1PvdMAdhDEXBmTt1E79QtRtE"
#define psw2 "9H1e5nrF8SY52npAEnuaLGTdnaUasVdVYUvKkqRWR4Nk"

#define str std::to_string
using namespace Pistache;
using cv::String;

void recognize(rapidjson::Document &doc, const MHandle &handle, cv::Mat &img, MRESULT &res)
{
    ASF_MultiFaceInfo detectedFaces;
    if (img.cols % 4 != 0) //4-bytes alignment
    {
        cv::Mat align = cv::Mat::zeros(img.rows, (4 - img.cols % 4), img.type());
        img = img.t();
        align = align.t();
        img.push_back(align);
        img = img.t();
    }

    res = ASFDetectFaces(handle, img.cols, img.rows, ASVL_PAF_RGB24_B8G8R8, (MUInt8 *)img.ptr<uchar>(0), &detectedFaces);
    if (res != MOK)
    {
        return;
    }
    auto &allocator = doc.GetAllocator();
    doc.AddMember("count", detectedFaces.faceNum, allocator);
    doc.AddMember("boxes", rapidjson::Value(rapidjson::kArrayType), allocator);
    
    for(int i = 0,a,b,c,d;i < detectedFaces.faceNum; i+=1){
        rapidjson::Value array(rapidjson::kArrayType);
        a = detectedFaces.faceRect[i].left;
        b = detectedFaces.faceRect[i].top;
        c = detectedFaces.faceRect[i].right;
        d = detectedFaces.faceRect[i].bottom;
        array.PushBack(a, allocator);
        array.PushBack(b, allocator);
        array.PushBack(c, allocator);
        array.PushBack(d, allocator);
        doc["boxes"].PushBack(array, allocator);
    }
}

class StatsEndpoint
{
public:
    StatsEndpoint(Address addr)
        : httpEndpoint(std::make_shared<Http::Endpoint>(addr))
    {
    }

    void init(MInt32 mask, size_t thr = 5)
    {
        auto opts = Http::Endpoint::options().threads(thr).maxRequestSize(1e9);
        std::vector<MRESULT> results;
        for (int i = 0; i < thr; i += 1)
        {
            MHandle handle = nullptr;
            auto res = ASFInitEngine(ASF_DETECT_MODE_IMAGE, ASF_OP_0_ONLY, 30, 50, mask, &handle);
            results.push_back(res);
            queue.push(handle);
        }
        if (std::any_of(results.begin(), results.end(), [&](MRESULT &res) { return res != MOK; }))
        {
            std::cout << "engines init failed" << std::endl;
        }
        httpEndpoint->init(opts);
        setupRoutes();
    }

    void start()
    {
        httpEndpoint->setHandler(router.handler());
        httpEndpoint->serve();
    }

private:
    void setupRoutes()
    {
        using namespace Rest;

        Routes::Post(router, "/detect", Routes::bind(&StatsEndpoint::doDetect, this));
        Routes::Get(router, "/ping", Routes::bind(&StatsEndpoint::doPing, this));
    }
    void doPing(const Rest::Request &request, Http::ResponseWriter response)
    {
        rapidjson::Document doc;
        doc.SetObject();
        rapidjson::StringBuffer result;
        rapidjson::Writer<rapidjson::StringBuffer> writer(result);
        doc.AddMember("code", static_cast<int>(Http::Code::Ok), doc.GetAllocator());
        
        doc.Accept(writer);
        response.send(Http::Code::Ok, std::string(result.GetString()));
    }

    void doDetect(const Rest::Request &request, Http::ResponseWriter response)
    {
        auto image_data = request.body();
        
        auto image_data_vector = std::vector<char>(image_data.begin(), image_data.end());
        auto image = cv::imdecode(image_data_vector, cv::IMREAD_ANYCOLOR);
        rapidjson::Document doc;
        doc.SetObject();
        rapidjson::StringBuffer result;
        rapidjson::Writer<rapidjson::StringBuffer> writer(result);

        MHandle handle = nullptr;
        MRESULT res = -1;
        queue.wait_and_pop(handle);
        recognize(doc, handle, image, res);
        if (res != MOK)
        {
            doc.AddMember("code", static_cast<int>(Http::Code::Not_Acceptable), doc.GetAllocator());

            doc.Accept(writer);
            response.send(Http::Code::Not_Acceptable, std::string(result.GetString()));
        }
        doc.AddMember("code", static_cast<int>(Http::Code::Ok), doc.GetAllocator());
        queue.push(handle);

        doc.Accept(writer);
        response.send(Http::Code::Ok, std::string(result.GetString()));
    }

    shared_queue<MHandle> queue;
    std::shared_ptr<Http::Endpoint> httpEndpoint;
    Rest::Router router;
};

int main(int argc, char *argv[])
{
    auto res = ASFActivation(psw1, psw2);
    if (res != MOK && res != MERR_ASF_ALREADY_ACTIVATED)
    {
        std::cout << res << " wdnmd" << std::endl;
        return 0;
    }
    else
    {
        std::cout << "activation successful" << std::endl;
    }
    MInt32 mask = ASF_FACE_DETECT;

    Port port(11459);
    int thr = 5;
    if (argc >= 2)
    {
        port = std::stol(argv[1]);

        if (argc == 3)
            thr = std::stol(argv[2]);
    }
    Address addr(Ipv4::any(), port);
    std::cout << "Cores = " << hardware_concurrency() << std::endl;
    std::cout << "Using " << thr << " threads" << std::endl;

    StatsEndpoint stats(addr);
    stats.init(mask, thr);

    stats.start();
    return 0;
}