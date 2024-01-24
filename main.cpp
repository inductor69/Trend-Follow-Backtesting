#include<iostream>
#include<vector>
#include<queue>
#include<map>
#include<fstream>
#include<string>
#include <sstream>
#include<thread>
#include <mutex> 
#include<memory>

#define DEBUG false

std::mutex stdOutMutex;

template<typename T>
void printMessage(T msg, bool nextLine = true){
    stdOutMutex.lock();
    std::cout<<msg;
    if(nextLine)std::cout<<std::endl;
    else std::cout<<" ";
    stdOutMutex.unlock();
} 

struct DataPoint{
    std::string date;
    float openPrice;
    float closePrice;
    long long int volume;

    void setData(const std::string &line){
        std::string word;
        std::stringstream s(line);
        int column=0;
        while (getline(s, word,',')) {
            if(column==0)date = word;
            if(column==1)openPrice = stof(word);
            if(column==4)closePrice = stof(word);
            if(column==6)volume = stoll(word);
            column++;
        }
    }
};


class Data{
    public:
        std::string symbolName;
        std::vector<DataPoint> symbolData;
        int numberOfRows;

        Data(std::string symbolName, std::string fileName)
            : symbolName(symbolName), numberOfRows(0){
            parseData(fileName);
        }

        void parseData(const std::string &fileName){
            numberOfRows = 0;
            symbolData.clear();

            std::fstream fin;
            fin.open(fileName, std::ios::in);
            std::string temp,line;

            while (fin >> temp) {
                DataPoint row;
                row.setData(temp);
                symbolData.push_back(row);
                numberOfRows++;
            }
        }

        void printData(){
            for(int row=0;row<numberOfRows;row++){
                std::cout<<symbolData[row].date<<", "<<symbolData[row].openPrice<<", "<<symbolData[row].closePrice<<", "<<symbolData[row].volume<<std::endl;
            }
        }

        ~Data(){
            if(DEBUG)printMessage("Deconstructing Data Object");
        }
};


class Strategy{
    public:
        std::string strategyName;
        std::map<std::string,int> strategyParams;

        virtual int8_t* getTradeSignals(const Data &data) = 0;

        static float findPercentageChange(const float &startValue, const float &endValue){
            return ((endValue-startValue)*100)/startValue;
        }

        virtual ~Strategy(){
            
        }
};


class TrendFollowingStrategy: public Strategy{
    public:
        TrendFollowingStrategy(std::string strategyName){
            this->strategyName=strategyName;
            strategyParams["LOOKBACK_PERIOD"]=90;
            strategyParams["ENTER_TRIGGER_PERCENTAGE"]=5;
            strategyParams["EXIT_TRIGGER_PERCENTAGE"]=5;
            strategyParams["TARGET_PERCENTAGE"]=20;
            strategyParams["STOP_LOSS_PERCENTAGE"]=10;
        }

        TrendFollowingStrategy(std::string strategyName, int lookBackPeriod, int entryTrigger, int exitTrigger,int targetPercentage, int stopLoss){
            this->strategyName=strategyName;
            strategyParams["LOOKBACK_PERIOD"]=lookBackPeriod;
            strategyParams["ENTER_TRIGGER_PERCENTAGE"]=entryTrigger;
            strategyParams["EXIT_TRIGGER_PERCENTAGE"]=exitTrigger;
            strategyParams["TARGET_PERCENTAGE"]=targetPercentage;
            strategyParams["STOP_LOSS_PERCENTAGE"]=stopLoss;
        }

        static float* getKMax(const Data &data, int K){
            std::deque<int> Qi(K);
            int i,N=data.numberOfRows;
            float mx=INT_MIN;
            float *kmax = new float[N];

            for (i=0; i < N; ++i) {
                while ((!Qi.empty()) && Qi.front() <= i - K)
                    Qi.pop_front();
                while ((!Qi.empty()) && data.symbolData[i].closePrice >= data.symbolData[Qi.back()].closePrice)
                    Qi.pop_back();
                Qi.push_back(i);
                kmax[i]=data.symbolData[Qi.front()].closePrice;
            }

            return kmax;
        }

        static float* getKMin(const Data &data, int K){
            std::deque<int> Qi(K);
            int i,N=data.numberOfRows;
            float mn=INT_MAX;
            float *kmin = new float[N];

            for (i=0; i < N; ++i) {
                while ((!Qi.empty()) && Qi.front() <= i - K)
                    Qi.pop_front();
                while ((!Qi.empty()) && data.symbolData[i].closePrice <= data.symbolData[Qi.back()].closePrice)
                    Qi.pop_back();
                Qi.push_back(i);
                kmin[i]=data.symbolData[Qi.front()].closePrice;
            }

            return kmin;
        }

        int8_t* getTradeSignals(const Data &data){
            enum TradeState{ NO_POSITION=0, LONG_POSITION=1, SHORT_POSITION=-1 };

            int8_t* signals = new int8_t[data.numberOfRows];
            TradeState state = NO_POSITION;
            int tradePrice = 0;

            float* kmax = TrendFollowingStrategy::getKMax(data, strategyParams["LOOKBACK_PERIOD"]);
            float* kmin = TrendFollowingStrategy::getKMin(data, strategyParams["LOOKBACK_PERIOD"]);

            for(int i=0;i<data.numberOfRows;i++){
                int maxPercentIncrease = findPercentageChange(kmin[i],data.symbolData[i].closePrice);
                int maxPercentDecrease = -1 * findPercentageChange(kmax[i],data.symbolData[i].closePrice);
                
                if(state==NO_POSITION){
                    if(maxPercentIncrease >= strategyParams["ENTER_TRIGGER_PERCENTAGE"]){
                        state=LONG_POSITION;
                        signals[i]=1;
                        tradePrice = data.symbolData[i].closePrice;
                    }
                    else if(maxPercentDecrease >= strategyParams["ENTER_TRIGGER_PERCENTAGE"]){
                        state=SHORT_POSITION;
                        signals[i]=2;
                        tradePrice = data.symbolData[i].closePrice;
                    }
                    else{
                        signals[i]=0;
                    }
                }
                else if(state==LONG_POSITION){
                    if(findPercentageChange(tradePrice,data.symbolData[i].closePrice)>=strategyParams["TARGET_PERCENTAGE"] || 
                        maxPercentDecrease >= strategyParams["EXIT_TRIGGER_PERCENTAGE"] || 
                        (-1*findPercentageChange(tradePrice,data.symbolData[i].closePrice)>=strategyParams["STOP_LOSS_PERCENTAGE"])){
                        state=NO_POSITION;
                        signals[i]=-1;
                    }
                    else signals[i]=0;
                }
                else if(state==SHORT_POSITION){
                    if((-1*findPercentageChange(tradePrice,data.symbolData[i].closePrice))>=strategyParams["TARGET_PERCENTAGE"] || 
                        maxPercentIncrease >= strategyParams["EXIT_TRIGGER_PERCENTAGE"] || 
                        (findPercentageChange(tradePrice,data.symbolData[i].closePrice)>=strategyParams["STOP_LOSS_PERCENTAGE"])){
                        state=NO_POSITION;
                        signals[i]=-2;
                    }
                    else signals[i]=0;
                }
            }

            delete[] kmax;
            delete[] kmin;

            return signals;
        }

        ~TrendFollowingStrategy(){
            strategyParams.clear();
            if(DEBUG)printMessage("Deconstructing Strategy Object");
        }
};


class Backtest{
    Strategy *strategyInstance;
    std::shared_ptr<Data> dataInstance;
    int8_t* tradeSignals;

    public:

        Backtest(Strategy *strategyInstance, std::shared_ptr<Data> dataInstance)
            :strategyInstance(strategyInstance), dataInstance(dataInstance), tradeSignals(nullptr){
            
        }

        void printResults(const int &totalTrades, const int &numOfProfitableTrades, const float &totalProfitPercent){
            stdOutMutex.lock();
            std::cout<<"\n*************************************************************\n";
            std::cout<<"Symbol: "<<dataInstance->symbolName<<std::endl;
            std::cout<<"Strategy: "<<strategyInstance->strategyName<<std::endl;
            std::cout<<"Total Trades Taken: "<<totalTrades<<std::endl;
            std::cout<<"Number Of Profitable Trades: "<<numOfProfitableTrades<<std::endl;
            std::cout<<"Total Profit Percentage: "<<totalProfitPercent<<std::endl;
            std::cout<<"Average Profit Percentage Per Trade: "<<float(totalProfitPercent)/float(totalTrades)<<std::endl;
            std::cout<<"*************************************************************\n";
            stdOutMutex.unlock();
        }

        void evaluateResults(){
            float openPrice;
            float totalProfitPercent=0;
            int numOfProfitableTrades=0,totalTrades=0;
            for(int i=0;i<dataInstance->numberOfRows;i++){
                if(tradeSignals[i]==1){
                    openPrice = dataInstance->symbolData[i].closePrice;
                }
                else if(tradeSignals[i]==-1){
                    float profit = Strategy::findPercentageChange(openPrice,dataInstance->symbolData[i].closePrice);
                    totalProfitPercent+=profit;
                    totalTrades+=1;
                    if(profit>0)numOfProfitableTrades+=1;
                }
                else if(tradeSignals[i]==2){
                    openPrice = dataInstance->symbolData[i].closePrice;
                }
                else if(tradeSignals[i]==-2){
                    float profit = Strategy::findPercentageChange(dataInstance->symbolData[i].closePrice, openPrice);
                    totalProfitPercent+=profit;
                    totalTrades+=1;
                    if(profit>0)numOfProfitableTrades+=1;
                }
            }
            printResults(totalTrades,numOfProfitableTrades,totalProfitPercent);
        }

        void runBacktest(){
            tradeSignals = strategyInstance->getTradeSignals(*dataInstance);
            evaluateResults();
        }

        ~Backtest(){
            delete tradeSignals;
            if(DEBUG)printMessage("Deconstructing Backtest Object");
        }
};


class Driver{
    int NUM_OF_THREADS;
    std::queue<std::pair<std::string,std::string> > symbolInputs;
    

    public:
        std::mutex queueMutex;
        Strategy *strategyInstance;

        Driver()
            : NUM_OF_THREADS(5){
        }

        Driver(int NUM_OF_THREADS)
            : NUM_OF_THREADS(NUM_OF_THREADS){

        }

        void setStrategyInstance(Strategy *strategyInstance){
            this->strategyInstance = strategyInstance;
        }

        void setSymbolInputs(){
            symbolInputs.push(std::make_pair("Meta", "data/META.csv"));
            symbolInputs.push(std::make_pair("Tesla", "data/TSLA.csv"));
            symbolInputs.push(std::make_pair("Amazon", "data/AMZN.csv"));
            symbolInputs.push(std::make_pair("Apple", "data/AAPL.csv"));
            symbolInputs.push(std::make_pair("Google", "data/GOOG.csv"));
        }

        void setSymbolInputs(std::queue<std::pair<std::string,std::string> > &symbolInputs){
            this->symbolInputs = symbolInputs;
        }

        static void processSymbol(const std::pair<std::string,std::string> &symbolInput, Strategy *strategyInstance){
            if(DEBUG)printMessage("Thread Strated For Symbol: "+symbolInput.first);
            std::shared_ptr<Data> symbolData = std::make_shared<Data>(symbolInput.first, symbolInput.second);
            Backtest *backtestInstance = new Backtest(strategyInstance, symbolData);
            backtestInstance->runBacktest();
            if(DEBUG)printMessage("Thread Completed For Symbol: "+symbolInput.first);
            delete backtestInstance;
        }

        static void runThreadTask(Driver *driverInstance){
            while(driverInstance->symbolInputs.size()){
                driverInstance->queueMutex.lock();
                if(driverInstance->symbolInputs.size()){
                    std::pair<std::string,std::string> symbolInput = driverInstance->symbolInputs.front();
                    driverInstance->symbolInputs.pop();
                    driverInstance->queueMutex.unlock();
                    processSymbol(symbolInput, driverInstance->strategyInstance);
                }
                else{
                    driverInstance->queueMutex.unlock();
                }
            }
        }

        void runBacktest(){
            std::shared_ptr<std::thread> threadsArray[NUM_OF_THREADS];
            for(int i=0;i<NUM_OF_THREADS;i++){
                std::shared_ptr<std::thread> threadInstance= std::make_shared<std::thread>(Driver::runThreadTask, this);
                threadsArray[i]=threadInstance;
            }
            for(int i=0;i<NUM_OF_THREADS;i++){
                threadsArray[i]->join();
            }
        }

        ~Driver(){
            if(DEBUG)printMessage("Deconstructing Driver Object");
        }
};



int main()
{
    Driver *driverInstance = new Driver();
    Strategy *strategyInstance = new TrendFollowingStrategy("Trend Following Strategy");

    driverInstance->setSymbolInputs();
    driverInstance->setStrategyInstance(strategyInstance);
    driverInstance->runBacktest();

    delete driverInstance;
    delete strategyInstance;

    return 0;
}