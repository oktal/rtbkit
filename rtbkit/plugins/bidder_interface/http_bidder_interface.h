/* http_bidder_interface.h                                         -*- C++ -*-
   Eric Robert, 2 April 2014
   Copyright (c) 2011 Datacratic.  All rights reserved.
*/

#pragma once

#include "rtbkit/common/bidder_interface.h"
#include "soa/service/http_client.h"
#include "soa/service/logs.h"

namespace RTBKIT {

struct Bids;

struct HttpBidderInterface : public BidderInterface
{
    HttpBidderInterface(std::string serviceName = "bidderService",
                        std::shared_ptr<ServiceProxies> proxies = std::make_shared<ServiceProxies>(),
                        Json::Value const & json = Json::Value());
    ~HttpBidderInterface();

    void start();
    void shutdown();
    void sendAuctionMessage(std::shared_ptr<Auction> const & auction,
                            double timeLeftMs,
                            std::map<std::string, BidInfo> const & bidders);

    void sendWinLossMessage(MatchedWinLoss const & event);

    void sendLossMessage(std::string const & agent,
                         std::string const & id);

    void sendCampaignEventMessage(std::string const & agent,
                                  MatchedCampaignEvent const & event);

    void sendBidLostMessage(std::string const & agent,
                            std::shared_ptr<Auction> const & auction);

    void sendBidDroppedMessage(std::string const & agent,
                               std::shared_ptr<Auction> const & auction);

    void sendBidInvalidMessage(std::string const & agent,
                               std::string const & reason,
                               std::shared_ptr<Auction> const & auction);

    void sendNoBudgetMessage(std::string const & agent,
                             std::shared_ptr<Auction> const & auction);

    void sendTooLateMessage(std::string const & agent,
                            std::shared_ptr<Auction> const & auction);

    void sendMessage(std::string const & agent,
                     std::string const & message);

    void sendErrorMessage(std::string const & agent,
                          std::string const & error,
                          std::vector<std::string> const & payload);

    void sendPingMessage(std::string const & agent,
                         int ping);

    virtual void tagRequest(OpenRTB::BidRequest &request,
                            const std::map<std::string, BidInfo> &bidders) const;

    static Logging::Category print;
    static Logging::Category error;
    static Logging::Category trace;
    
private:

    struct AgentBidsInfo {
        std::shared_ptr<const AgentConfig> agentConfig;
        std::string agentName;
        Id auctionId;
        Bids bids;
        WinCostModel wcm;
    };

    typedef std::map<std::string, AgentBidsInfo> AgentBids;

    MessageLoop loop;
    std::shared_ptr<HttpClient> httpClientRouter;
    std::shared_ptr<HttpClient> httpClientAdserverWins;
    std::shared_ptr<HttpClient> httpClientAdserverEvents;
    std::string routerHost;
    std::string routerPath;
    std::string adserverHost;
    uint16_t adserverWinPort;
    uint16_t adserverEventPort;

    void submitBids(AgentBids &info, size_t impressionsCount);
    bool prepareRequest(OpenRTB::BidRequest &request,
                        const RTBKIT::BidRequest &originalRequest,
                        const std::shared_ptr<Auction> &auction,
                        const std::map<std::string, BidInfo> &bidders) const;
    void injectBids(const std::string &agent, Id auctionId,
                    const Bids &bids, WinCostModel wcm);

};

}

