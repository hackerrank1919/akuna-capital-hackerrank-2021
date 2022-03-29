#include <map>
#include <set>
#include <list>
#include <cmath>
#include <ctime>
#include <deque>
#include <queue>
#include <stack>
#include <string>
#include <bitset>
#include <cstdio>
#include <limits>
#include <vector>
#include <climits>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <numeric>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <unordered_map>

namespace engine {

namespace {

using PriceType = uint32_t;
using QuantityType = uint32_t;
using OrderId = std::string;

constexpr size_t EXPECTED_ORDER_COULMNS = 5;
constexpr size_t EXPECTED_CANCEL_COLUMNS = 2;
constexpr size_t EXPECTED_MODIFY_COLUMNS = 5;

enum class TradeOperation
{
    Unknown = 0,
    Sell,
    Buy,
};

enum class OrderType
{
    Unknown = 0,
    GFD,
    IOC,
};

} // namespace

class ColumnsData final
{
public:
    void parseLine(std::string&& line) noexcept
    {
        lineData = std::move(line);

        index = 0;

        items.clear();
        items.emplace_back(nullptr, nullptr);

        const auto endIt = lineData.end();
        for (auto charIt = lineData.begin(); charIt != endIt; ++charIt)
        {
            if (*charIt == ' ')
            {
                auto& item = items.back();
                if (item.first != nullptr && item.second == nullptr)
                {
                    item.second = &(*charIt);
                    items.emplace_back(nullptr, nullptr);
                }
            }
            else
            {
                auto& item = items.back();
                if (item.first == nullptr)
                {
                    item.first = &(*charIt);
                }
            }
        }

        auto& item = items.back();
        if (item.first == nullptr)
        {
            items.pop_back();
        }
        else if (item.second == nullptr)
        {
            item.second = &lineData.data()[lineData.size()];
        }
    }

    uint32_t getColumnsCount() const noexcept { return items.size(); }

    int getIntegerAndForward() noexcept
    {
        if (index >= items.size())
            return 0;

        return atoi(items[index++].first);
    }

    bool compare(const char* str, size_t size) const noexcept
    {
        if (index >= items.size())
            return false;

        auto& item = items[index];

        if (size_t(std::distance(item.first, item.second)) != size)
            return false;

        return memcmp(item.first, str, size) == 0;
    }

    void forward() noexcept { ++index; }

    std::string getStringAndForward() noexcept
    {
        if (index >= items.size())
            return {};

        auto& item = items[index++];

        return std::string(item.first, item.second);
    }
    
    bool empty() const noexcept { return items.empty(); }
    
private:
    using Item = std::pair<const char*, const char*>;
    using Items = std::vector<Item>;

    std::string lineData;
    Items items;
    uint32_t index{0};
};

TradeOperation getTradeOperation(const ColumnsData& columns) noexcept
{
    if (columns.compare("BUY", 3))
        return TradeOperation::Buy;
        
    if (columns.compare("SELL", 4))
        return TradeOperation::Sell;
        
    return TradeOperation::Unknown;
}

OrderType getOrderType(const ColumnsData& columns) noexcept
{
    if (columns.compare("GFD", 3))
        return OrderType::GFD;
        
    if (columns.compare("IOC", 3))
        return OrderType::IOC;
        
    return OrderType::Unknown;
}

class Order final
{
public:
    explicit Order(ColumnsData& columns) noexcept
    {
        if (columns.getColumnsCount() != EXPECTED_ORDER_COULMNS)
            return; // error

        tradeOperation = getTradeOperation(columns);
        if (tradeOperation == TradeOperation::Unknown)
            return;
      
        columns.forward(); // 1
        
        orderType = getOrderType(columns);
        if (orderType == OrderType::Unknown)
            return;
       
        columns.forward(); // 2

        const auto newPrice = columns.getIntegerAndForward();
        if (newPrice <= 0)
            return;
            
        price = PriceType(newPrice);

        const auto newQuantity = columns.getIntegerAndForward();
        if (newQuantity <= 0)
            return;
            
        quantity = QuantityType(newQuantity);
        
        orderId = columns.getStringAndForward();
        
        valid = true;
    }
    
    explicit Order(const OrderId& id, const PriceType newPrice,
                    const QuantityType newQuantity, const TradeOperation operation) noexcept :
                    orderId(id), price(newPrice), quantity(newQuantity),
                    orderType(OrderType::GFD), tradeOperation(operation), valid(true)
    {}
    
    bool isValidFormat() const noexcept { return valid; }
    const OrderId& getOrderId() const noexcept { return orderId; }
    PriceType getPrice() const noexcept { return price; }
    QuantityType getQuantity() const noexcept { return quantity; }
    bool isInsertOrCancel() const noexcept { return orderType == OrderType::IOC; }
    bool isBuying() const noexcept { return tradeOperation == TradeOperation::Buy; }
    
    void subtract(const QuantityType subtractQuantity) noexcept
    {
        quantity -= subtractQuantity;
    }
    
private:
    OrderId orderId;
    PriceType price{0};
    QuantityType quantity{0};
    OrderType orderType{OrderType::Unknown};
    TradeOperation tradeOperation{TradeOperation::Unknown};
    bool valid{false};
};

class OrdersData final
{
public:
    using OrdersList = std::vector<Order>;
    
    OrdersData() noexcept
    {
        orders.reserve(100);
    }
    
    OrdersData(OrdersData&&) = default;
    OrdersData& operator=(OrdersData&&) = default;

    OrdersData(const OrdersData&) = delete;
    OrdersData& operator=(const OrdersData&) = delete;

    void addOrder(Order&& order) noexcept
    {
        totalQuantity += order.getQuantity();
        orders.emplace_back(std::move(order));
    }
    
    void eraseOrder(const OrderId& orderId) noexcept
    {
        auto orderIt = std::find_if(orders.begin(), orders.end(),
            [&orderId](const Order& order) noexcept { return order.getOrderId() == orderId; });
            
        if (orderIt == orders.end())
            return;
            
        totalQuantity -= orderIt->getQuantity();
        orders.erase(orderIt);
    }

    void eraseOrder(const uint32_t index) noexcept
    {
        if (index >= orders.size())
            return;
            
        totalQuantity -= orders[index].getQuantity();
        orders.erase(orders.begin() + index);
    }
    
    void subtract(const uint32_t index, const QuantityType quantity) noexcept
    {
        if (index >= orders.size())
            return;
            
        auto& order = orders[index];
        
        order.subtract(quantity);
            
        if (order.getQuantity() == 0)
        {
            eraseOrder(index);
        }
        else
        {
            totalQuantity -= quantity;
        }
    }
    
    bool empty() const noexcept { return orders.empty(); }
    const Order& getFront() const noexcept { return orders.front(); }
    QuantityType getQuantity() const noexcept { return totalQuantity; }

private:
    OrdersList orders;
    QuantityType totalQuantity{0};
};

class MatchingEngine final
{
public:
    void runCommand(std::string&& line) noexcept
    {
        columnsData.parseLine(std::move(line));
        if (columnsData.empty())
            return;
            
        if (columnsData.compare("CANCEL", 6))
        {
            if (columnsData.getColumnsCount() == EXPECTED_CANCEL_COLUMNS)
            {
                columnsData.forward();
                cancelOrder(columnsData.getStringAndForward());
            }
        }
        else if (columnsData.compare("MODIFY", 6))
        {
            if (columnsData.getColumnsCount() == EXPECTED_MODIFY_COLUMNS)
                modifyOrder();
        }
        else if (columnsData.compare("PRINT", 5))
        {
            printOrders();
        }
        else
        {
            auto order = Order(columnsData);
            if (!order.isValidFormat())
                return;
                
            placeOrder(std::move(order));
        }
    }
    
private:
    using PricesMap = std::map<PriceType, OrdersData>;
    using OrderToPrice = std::unordered_map<OrderId, PriceType>;
    using PricesList = std::list<PriceType>;

    void placeOrder(Order&& order) noexcept
    {
        if (isOrderExists(order.getOrderId()))
            return;
            
        if (trade(order))
            return;
        
        if (order.isInsertOrCancel())
            return;
        
        if (!orderToPrice.emplace(order.getOrderId(), order.getPrice()).second)
            return;
        
        if (order.isBuying())
        {
            buyPrices[order.getPrice()].addOrder(std::move(order));
        }
        else
        {
            sellPrices[order.getPrice()].addOrder(std::move(order));
        }
    }
    
    bool trade(Order& order) noexcept
    {
        bool result = false;

        if (order.isBuying())
        {
            QuantityType quantityTraded = order.getQuantity();
            do
            {
                if (sellPrices.empty())
                    break;
                    
                quantityTraded = order.getQuantity();
                
                const auto end = sellPrices.end();
                for (auto priceIt = sellPrices.begin(); priceIt != end; ++priceIt)
                {
                    if (priceIt->first > order.getPrice())
                        break;
                        
                    if (trade(order, priceIt->second))
                    {
                        result = true;
                        break;
                    }
                }

                if (result)
                    break;
            }
            while (quantityTraded != order.getQuantity());
        }
        else
        {
            QuantityType quantityTraded = order.getQuantity();
            do
            {
                if (buyPrices.empty())
                    break;
                    
                quantityTraded = order.getQuantity();
                
                const auto end = buyPrices.rend();
                for (auto priceIt = buyPrices.rbegin(); priceIt != end; ++priceIt)
                {   
                    if (priceIt->first < order.getPrice())
                        break;
                    
                    if (trade(order, priceIt->second))
                    {
                        result = true;
                        break;
                    }
                }
                
                if (result)
                    break;
            }
            while (quantityTraded != order.getQuantity());
        }
        
        return result;
    }
    
    bool trade(Order& order, OrdersData& ordersData) noexcept
    {
        while (!ordersData.empty())
        {
            auto& orderIt = ordersData.getFront();
            
            if (orderIt.getQuantity() > order.getQuantity())
            {
                printTrade(order, orderIt);
                ordersData.subtract(0, order.getQuantity());
                return true;
            }
            else if (orderIt.getQuantity() < order.getQuantity())
            {
                printTrade(order, orderIt);
                order.subtract(orderIt.getQuantity());

                orderToPrice.erase(orderIt.getOrderId());
                ordersData.eraseOrder(0);
            }
            else
            {
                printTrade(order, orderIt);
  
                orderToPrice.erase(orderIt.getOrderId());
                ordersData.eraseOrder(0);
                return true;
            }
        }
        
        return false;
    }
    
    void cancelOrder(const OrderId& orderId) noexcept
    {
        auto orderPriceIt = orderToPrice.find(orderId);
        if (orderPriceIt == orderToPrice.end())
            return;
            
        removeOrderInMap(orderId, orderPriceIt->second, buyPrices);
        removeOrderInMap(orderId, orderPriceIt->second, sellPrices);
        
        orderToPrice.erase(orderPriceIt);
    }

    void removeOrderInMap(const OrderId& orderId, const PriceType price, PricesMap& pricesMap) noexcept
    {
        auto priceIt = pricesMap.find(price);
        if (priceIt != pricesMap.end())
        {
            priceIt->second.eraseOrder(orderId);
        }
    }
    
    void printOrders() const noexcept
    {
        printPricesMap(sellPrices, "SELL:");
        printPricesMap(buyPrices, "BUY:");
    }
    
    void printPricesMap(const PricesMap& pricesMap, const std::string& group) const noexcept
    {  
        std::cout << group << '\n';
        
        for (auto priceIt = pricesMap.rbegin(); priceIt != pricesMap.rend(); ++priceIt)
        {
            if (priceIt->second.getQuantity() > 0)
                std::cout << priceIt->first << ' ' << priceIt->second.getQuantity() << '\n';
        }
    }
    
    void printTrade(const Order& order1, const Order& order2) const noexcept
    {
        const auto tradeQuantity = std::min(order1.getQuantity(), order2.getQuantity());
        std::cout << "TRADE "
                    << order2.getOrderId() << ' ' << order2.getPrice() << ' ' << tradeQuantity << ' '
                    << order1.getOrderId() << ' ' << order1.getPrice() << ' ' << tradeQuantity
                    << '\n';
    }
    
    void modifyOrder() noexcept
    {
        columnsData.forward();
        
        const auto orderId = columnsData.getStringAndForward();
        if (!isOrderExists(orderId))
            return;
            
        const auto newTradeOperation = getTradeOperation(columnsData);
        if (newTradeOperation == TradeOperation::Unknown)
            return;
            
        columnsData.forward();
        
        const auto newPrice = columnsData.getIntegerAndForward();
        if (newPrice <= 0)
            return;

        const auto newQuantity = columnsData.getIntegerAndForward();
        if (newQuantity <= 0)
            return;

        cancelOrder(orderId);

        placeOrder(Order(orderId, newPrice, newQuantity, newTradeOperation));
    }
    
    bool isOrderExists(const std::string& orderId) const noexcept
    {
        return orderToPrice.count(orderId) > 0;
    }

    ColumnsData columnsData;
    PricesMap buyPrices;
    PricesMap sellPrices;
    OrderToPrice orderToPrice;
};

} // namespace engine

int main() {
    engine::MatchingEngine matchingEngine;

    for (std::string inputLine; std::getline(std::cin, inputLine); )
    {
        matchingEngine.runCommand(std::move(inputLine));
    }
    return 0;
}