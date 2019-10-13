#include <pistache/http.h>
#include <pistache/router.h>
#include <pistache/endpoint.h>
#include <iostream>

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

#define APPID "txwUMMgDJr1SZurx1HV1PvdMAdhDEXBmTt1E79QtRtE"
#define SDKKey "9H1e5nrF8SY52npAEnuaLGTdnaUasVdVYUvKkqRWR4Nk"

using namespace Pistache;
using cv::String;

std::string recognize(const MHandle &handle, cv::Mat &img, MRESULT &res)
{
    ASF_MultiFaceInfo detectedFaces;
    ASF_SingleFaceInfo SingleDetectedFaces;

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
        return std::string("failed");
    }
    SingleDetectedFaces.faceRect.left = detectedFaces.faceRect[0].left;
    SingleDetectedFaces.faceRect.top = detectedFaces.faceRect[0].top;
    SingleDetectedFaces.faceRect.right = detectedFaces.faceRect[0].right;
    SingleDetectedFaces.faceRect.bottom = detectedFaces.faceRect[0].bottom;
    SingleDetectedFaces.faceOrient = detectedFaces.faceOrient[0];
    int a = SingleDetectedFaces.faceRect.left;
    int b = SingleDetectedFaces.faceRect.right;
    int c = SingleDetectedFaces.faceRect.top;
    int d = SingleDetectedFaces.faceRect.bottom;
    std::stringstream ss;
    ss << "[" << a << "," << c << "," << b << "," << d << "]";
    std::string result;
    ss >> result;
    return result;
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
            auto res = ASFInitEngine(ASF_DETECT_MODE_IMAGE, ASF_OP_0_ONLY, 30, 1, mask, &handle);
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
        std::string result = "{\"code\":\"200\"}";
        response.send(Http::Code::Ok, result);
    }

    void doDetect(const Rest::Request &request, Http::ResponseWriter response)
    {
        auto image_data = request.body();
        auto image_data_vector = std::vector<char>(image_data.begin(), image_data.end());
        auto image = cv::imdecode(image_data_vector, cv::IMREAD_ANYCOLOR);

        MHandle handle = nullptr;
        MRESULT res = -1;
        queue.wait_and_pop(handle);

        auto box = recognize(handle, image, res);
        if (res != MOK)
        {
            std::string result = "{\"data\":\"406\"}";
            response.send(Http::Code::Not_Acceptable, result);
        }

        queue.push(handle);
        std::string result = "{\"code\":\"200\",\"box\":" + box + "}";
        response.send(Http::Code::Ok, result);
    }

    shared_queue<MHandle> queue;
    std::shared_ptr<Http::Endpoint> httpEndpoint;
    Rest::Router router;
};

int main(int argc, char *argv[])
{
    auto res = ASFActivation(APPID, SDKKey);
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

    Port port(11451);
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
    try
    {
        stats.start();
    }
    catch (std::exception e)
    {
        std::cout << e.what() << std::endl;
        return 0;
    }
    return 0;
}