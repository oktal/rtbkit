/* post_auction_runner.cc
   JS Bejeau , 13 February 2014

   Copyright (c) 2013 Datacratic Inc.  All rights reserved.
*/

#include "post_auction_runner.h"
#include "post_auction_service.h"
#include "rtbkit/core/banker/slave_banker.h"
#include "soa/service/service_utils.h"
#include "soa/utils/print_utils.h"
#include "jml/utils/file_functions.h"

#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>

using namespace std;
using namespace boost::program_options;
using namespace Datacratic;
using namespace RTBKIT;

static Json::Value loadJsonFromFile(const std::string & filename)
{
    ML::File_Read_Buffer buf(filename);
    return Json::parse(std::string(buf.start(), buf.end()));
}

/************************************************************************/
/* POST AUCTION LOOP RUNNER                                             */
/************************************************************************/
PostAuctionRunner::
PostAuctionRunner() :
    shard(0),
    auctionTimeout(EventMatcher::DefaultAuctionTimeout),
    winTimeout(EventMatcher::DefaultWinTimeout),
    bidderConfigurationFile("rtbkit/examples/bidder-config.json"),
    winLossPipeTimeout(PostAuctionService::DefaultWinLossPipeTimeout),
    campaignEventPipeTimeout(PostAuctionService::DefaultCampaignEventPipeTimeout),
    useHttpBanker(false)
{
}

void
PostAuctionRunner::
doOptions(int argc, char ** argv,
        const boost::program_options::options_description & opts)
{
    using namespace boost::program_options;

    options_description postAuctionLoop_options("Post Auction Loop options");
    postAuctionLoop_options.add_options()
        ("bidder,b", value<string>(&bidderConfigurationFile),
         "configuration file with bidder interface data")
        ("use-http-banker", bool_switch(&useHttpBanker),
         "Communicate with the MasterBanker over http")
        ("shard,s", value<size_t>(&shard),
         "Shard index starting at 0 for this post auction loop")
        ("win-seconds", value<float>(&winTimeout),
         "Timeout for storing win auction")
        ("auction-seconds", value<float>(&auctionTimeout),
         "Timeout to get late win auction")
        ("winlossPipe-seconds", value<int>(&winLossPipeTimeout),
         "Timeout before sending error on WinLoss pipe")
        ("campaignEventPipe-seconds", value<int>(&campaignEventPipeTimeout),
         "Timeout before sending error on CampaignEvent pipe");

    options_description all_opt = opts;
    all_opt
        .add(serviceArgs.makeProgramOptions())
        .add(postAuctionLoop_options);

    all_opt.add_options()
        ("help,h","print this message");

    variables_map vm;
    store(command_line_parser(argc, argv)
            .options(all_opt)
            .run(),
            vm);
    notify(vm);

    if (vm.count("help")) {
        cerr << all_opt << endl;
        exit(1);
    }
}

void
PostAuctionRunner::
init()
{
    auto proxies = serviceArgs.makeServiceProxies();
    auto serviceName = serviceArgs.serviceName("PostAuctionLoop");

    auto bidderConfig = loadJsonFromFile(bidderConfigurationFile);

    postAuctionLoop = std::make_shared<PostAuctionService>(proxies, serviceName);
    postAuctionLoop->initBidderInterface(bidderConfig);
    postAuctionLoop->init(shard);

    postAuctionLoop->setWinTimeout(winTimeout);
    postAuctionLoop->setAuctionTimeout(auctionTimeout);
    postAuctionLoop->setWinLossPipeTimeout(winLossPipeTimeout);
    postAuctionLoop->setCampaignEventPipeTimeout(campaignEventPipeTimeout);

    LOG(PostAuctionService::print) << "win timeout is " << winTimeout << std::endl;
    LOG(PostAuctionService::print) << "auction timeout is " << auctionTimeout << std::endl;
    LOG(PostAuctionService::print) << "winLoss pipe timeout is " << winLossPipeTimeout << std::endl;
    LOG(PostAuctionService::print) << "campaignEvent pipe timeout is " << campaignEventPipeTimeout << std::endl;

    banker = std::make_shared<SlaveBanker>(postAuctionLoop->serviceName() + ".slaveBanker");
    std::shared_ptr<ApplicationLayer> layer;
    if (useHttpBanker) {
        auto bankerUri = proxies->bankerUri;
        ExcCheck(!bankerUri.empty(),
                "the banker-uri must be specified in the bootstrap.json");
        LOG(PostAuctionService::print) << "using http interface for the MasterBanker" << std::endl;
        layer = make_application_layer<HttpLayer>(bankerUri);
    }
    else {
        layer = make_application_layer<ZmqLayer>(proxies->config);
        LOG(PostAuctionService::print) << "using zmq interface for the MasterBanker" << std::endl;
    }
    banker->setApplicationLayer(layer);

    postAuctionLoop->addSource("slave-banker", *banker);
    postAuctionLoop->setBanker(banker);
    postAuctionLoop->bindTcp();

}

void
PostAuctionRunner::
start()
{
    postAuctionLoop->start();
}

void
PostAuctionRunner::
shutdown()
{
    postAuctionLoop->shutdown();
    banker->shutdown();
}



PostAuctionService::Stats
report( const PostAuctionService& service,
        double delta,
        const PostAuctionService::Stats& last = PostAuctionService::Stats())
{
    auto current = service.stats;

    auto diff = current;
    diff -= last;

    double bidsThroughput = diff.auctions / delta;
    double eventsThroughput = diff.events / delta;
    double winsThroughput = diff.matchedWins / delta;
    double lossThroughput = diff.matchedLosses / delta;

    std::stringstream ss;
    ss << std::endl
        << printValue(bidsThroughput) << " bids/sec\n"
        << printValue(eventsThroughput) << " events/sec\n"
        << printValue(winsThroughput) << " wins/sec\n"
        << printValue(lossThroughput) << " loss/sec\n"
        << printValue(current.unmatchedEvents) << " unmatched\n"
        << printValue(current.errors) << " errors\n";
    LOG(PostAuctionService::print) << ss.str();

    return current;
}

int main(int argc, char ** argv)
{

    PostAuctionRunner runner;

    runner.doOptions(argc, argv);
    runner.init();
    runner.start();

    auto stats = report(*runner.postAuctionLoop, 0.1);
    for (;;) {
        ML::sleep(10.0);
        stats = report(*runner.postAuctionLoop, 10.0, stats);
    }

}
