#include <iostream>
#include <vector>
#include <queue>
#include <map>
#include <fstream>
#include <string>
#include <sstream>
#include <thread>
#include <mutex>
#include <memory>

#define DEBUG false

std::mutex stdOutMutex;

template <typename T>
// This function prints a message to the standard output with an optional newline character.
void printMessage(T msg, bool nextLine = true)
{
    // Lock a mutex to ensure thread safety when printing.
    stdOutMutex.lock();

    // Print the message to the standard output.
    std::cout << msg;

    // Check if the 'nextLine' flag is set to true.
    if (nextLine)
    {
        // If true, add a newline character to the end of the message.
        std::cout << std::endl;
    }
    else
    {
        // If false, add a space character to the end of the message.
        std::cout << " ";
    }

    // Unlock the mutex to release the lock for other threads.
    stdOutMutex.unlock();
}

// Define a C++ struct named 'DataPoint' to store financial data.
struct DataPoint
{
    std::string date;     // Stores the date associated with the data point.
    float openPrice;      // Stores the opening price of the instrument.
    float closePrice;     // Stores the closing price of the instrument.
    long long int volume; // Stores the trading volume associated with the data point.

    // Member function to set the data members of the struct from a comma-separated string.
    void setData(const std::string &line)
    {
        std::string word;
        std::stringstream s(line);
        int column = 0;

        // Tokenize the input line using a comma as the delimiter.
        while (getline(s, word, ','))
        {
            // Check the column position to determine which member to assign the value to.
            if (column == 0)
                date = word; // Assign the first column (date) to 'date'.
            if (column == 1)
                openPrice = stof(word); // Assign the second column (openPrice) to 'openPrice'.
            if (column == 4)
                closePrice = stof(word); // Assign the fifth column (closePrice) to 'closePrice'.
            if (column == 6)
                volume = stoll(word); // Assign the seventh column (volume) to 'volume'.

            column++; // Move to the next column in the input line.
        }
    }
};

// Define a C++ class named 'Data' to handle financial data.
class Data
{
public:
    std::string symbolName;            // Stores the symbol name associated with the data.
    std::vector<DataPoint> symbolData; // Stores a collection of DataPoint objects.
    int numberOfRows;                  // Stores the number of rows in the data.

    // Constructor that takes symbol name and a file name to parse data.
    Data(std::string symbolName, std::string fileName)
        : symbolName(symbolName), numberOfRows(0)
    {
        parseData(fileName); // Call the parseData function to initialize the data.
    }

    // Member function to parse data from a file and populate the symbolData vector.
    void parseData(const std::string &fileName)
    {
        numberOfRows = 0;   // Initialize the row count to 0.
        symbolData.clear(); // Clear any existing data.

        std::fstream fin;                 // Create a file stream object.
        fin.open(fileName, std::ios::in); // Open the file for reading.
        std::string temp, line;

        // Loop to read data from the file.
        while (fin >> temp)
        {
            DataPoint row;             // Create a DataPoint object to store a row of data.
            row.setData(temp);         // Parse and set data for the row.
            symbolData.push_back(row); // Add the row to the symbolData vector.
            numberOfRows++;            // Increment the row count.
        }
    }

    // Member function to print the stored data to the console.
    void printData()
    {
        for (int row = 0; row < numberOfRows; row++)
        {
            std::cout << symbolData[row].date << ", " << symbolData[row].openPrice << ", " << symbolData[row].closePrice << ", " << symbolData[row].volume << std::endl;
        }
    }

    // Destructor for the class.
    ~Data()
    {
        // If DEBUG flag is set, print a message indicating deconstruction.
        if (DEBUG)
            printMessage("Deconstructing Data Object");
    }
};

// Define a C++ class named 'Strategy' for implementing trading strategies.
class Strategy
{
public:
    std::string strategyName;                  // Stores the name of the strategy.
    std::map<std::string, int> strategyParams; // Stores strategy-specific parameters.

    // Pure virtual function to get trade signals based on financial data.
    virtual int8_t *getTradeSignals(const Data &data) = 0;

    // Static function to calculate and return the percentage change between two values.
    static float findPercentageChange(const float &startValue, const float &endValue)
    {
        return ((endValue - startValue) * 100) / startValue;
    }

    // Virtual destructor for the class.
    virtual ~Strategy()
    {
        // Destructor is empty here; it can be overridden in derived classes.
    }
};

class TrendFollowingStrategy : public Strategy
{
public:
    TrendFollowingStrategy(std::string strategyName)
    {
        this->strategyName = strategyName;
        strategyParams["LOOKBACK_PERIOD"] = 90;
        strategyParams["ENTER_TRIGGER_PERCENTAGE"] = 5;
        strategyParams["EXIT_TRIGGER_PERCENTAGE"] = 5;
        strategyParams["TARGET_PERCENTAGE"] = 20;
        strategyParams["STOP_LOSS_PERCENTAGE"] = 10;
    }

    TrendFollowingStrategy(std::string strategyName, int lookBackPeriod, int entryTrigger, int exitTrigger, int targetPercentage, int stopLoss)
    {
        this->strategyName = strategyName;
        strategyParams["LOOKBACK_PERIOD"] = lookBackPeriod;
        strategyParams["ENTER_TRIGGER_PERCENTAGE"] = entryTrigger;
        strategyParams["EXIT_TRIGGER_PERCENTAGE"] = exitTrigger;
        strategyParams["TARGET_PERCENTAGE"] = targetPercentage;
        strategyParams["STOP_LOSS_PERCENTAGE"] = stopLoss;
    }

    // Function 'getKMax' calculates the maximum closing prices within a sliding window of size K
    // and returns an array of those maximum values.

    // Parameters:
    // - 'data': The financial data containing historical closing prices.
    // - 'K': The size of the sliding window.

    // Returns:
    // - A dynamically allocated array of floats containing the K-maximum closing prices.

    static float *getKMax(const Data &data, int K)
    {
        std::deque<int> Qi(K); // Create a deque to store indices within the sliding window.
        int i, N = data.numberOfRows;
        float mx = INT_MIN;         // Initialize a variable to track the maximum value.
        float *kmax = new float[N]; // Allocate memory for the K-maximum array.

        for (i = 0; i < N; ++i)
        {
            // Remove elements from the front of the deque if they are outside the window.
            while ((!Qi.empty()) && Qi.front() <= i - K)
                Qi.pop_front();

            // Remove elements from the back of the deque if they are less than the current closing price.
            while ((!Qi.empty()) && data.symbolData[i].closePrice >= data.symbolData[Qi.back()].closePrice)
                Qi.pop_back();

            Qi.push_back(i); // Add the current index to the deque.

            kmax[i] = data.symbolData[Qi.front()].closePrice; // Store the maximum value at the current index.
        }

        return kmax; // Return the array of K-maximum closing prices.
    }

    // Function 'getKMin' calculates the minimum closing prices within a sliding window of size K
    // and returns an array of those minimum values.

    // Parameters:
    // - 'data': The financial data containing historical closing prices.
    // - 'K': The size of the sliding window.

    // Returns:
    // - A dynamically allocated array of floats containing the K-minimum closing prices.

    static float *getKMin(const Data &data, int K)
    {
        std::deque<int> Qi(K); // Create a deque to store indices within the sliding window.
        int i, N = data.numberOfRows;
        float mn = INT_MAX;         // Initialize a variable to track the minimum value.
        float *kmin = new float[N]; // Allocate memory for the K-minimum array.

        for (i = 0; i < N; ++i)
        {
            // Remove elements from the front of the deque if they are outside the window.
            while ((!Qi.empty()) && Qi.front() <= i - K)
                Qi.pop_front();

            // Remove elements from the back of the deque if they are greater than the current closing price.
            while ((!Qi.empty()) && data.symbolData[i].closePrice <= data.symbolData[Qi.back()].closePrice)
                Qi.pop_back();

            Qi.push_back(i); // Add the current index to the deque.

            kmin[i] = data.symbolData[Qi.front()].closePrice; // Store the minimum value at the current index.
        }

        return kmin; // Return the array of K-minimum closing prices.
    }

    // Function 'getTradeSignals' calculates trading signals based on a trend-following strategy.
    // It evaluates historical data to determine buy (1) and sell (-1) signals for long and short positions.

    // Parameters:
    // - 'data': The financial data containing historical closing prices and volume.

    // Returns:
    // - A dynamically allocated array of int8_t values representing trading signals:
    //   - 1: Buy signal (long position)
    //   - -1: Sell signal (exit long position)
    //   - 2: Short signal (short position)
    //   - -2: Cover signal (exit short position)
    //   - 0: No action

    int8_t *getTradeSignals(const Data &data)
    {
        // Define trade states as an enum.
        enum TradeState
        {
            NO_POSITION = 0,
            LONG_POSITION = 1,
            SHORT_POSITION = -1
        };

        int8_t *signals = new int8_t[data.numberOfRows]; // Allocate memory for the signals.
        TradeState state = NO_POSITION;                  // Initialize the trade state to no position.
        int tradePrice = 0;                              // Initialize the trade price.

        // Calculate K-maximum and K-minimum arrays using trend-following strategy functions.
        float *kmax = TrendFollowingStrategy::getKMax(data, strategyParams["LOOKBACK_PERIOD"]);
        float *kmin = TrendFollowingStrategy::getKMin(data, strategyParams["LOOKBACK_PERIOD"]);

        for (int i = 0; i < data.numberOfRows; i++)
        {
            int maxPercentIncrease = findPercentageChange(kmin[i], data.symbolData[i].closePrice);
            int maxPercentDecrease = -1 * findPercentageChange(kmax[i], data.symbolData[i].closePrice);

            if (state == NO_POSITION)
            {
                if (maxPercentIncrease >= strategyParams["ENTER_TRIGGER_PERCENTAGE"])
                {
                    state = LONG_POSITION;
                    signals[i] = 1; // Set a buy signal for a long position.
                    tradePrice = data.symbolData[i].closePrice;
                }
                else if (maxPercentDecrease >= strategyParams["ENTER_TRIGGER_PERCENTAGE"])
                {
                    state = SHORT_POSITION;
                    signals[i] = 2; // Set a short signal for a short position.
                    tradePrice = data.symbolData[i].closePrice;
                }
                else
                {
                    signals[i] = 0; // No action.
                }
            }
            else if (state == LONG_POSITION)
            {
                if (findPercentageChange(tradePrice, data.symbolData[i].closePrice) >= strategyParams["TARGET_PERCENTAGE"] ||
                    maxPercentDecrease >= strategyParams["EXIT_TRIGGER_PERCENTAGE"] ||
                    (-1 * findPercentageChange(tradePrice, data.symbolData[i].closePrice) >= strategyParams["STOP_LOSS_PERCENTAGE"]))
                {
                    state = NO_POSITION;
                    signals[i] = -1; // Set a sell signal to exit the long position.
                }
                else
                {
                    signals[i] = 0; // No action.
                }
            }
            else if (state == SHORT_POSITION)
            {
                if ((-1 * findPercentageChange(tradePrice, data.symbolData[i].closePrice)) >= strategyParams["TARGET_PERCENTAGE"] ||
                    maxPercentIncrease >= strategyParams["EXIT_TRIGGER_PERCENTAGE"] ||
                    (findPercentageChange(tradePrice, data.symbolData[i].closePrice) >= strategyParams["STOP_LOSS_PERCENTAGE"]))
                {
                    state = NO_POSITION;
                    signals[i] = -2; // Set a cover signal to exit the short position.
                }
                else
                {
                    signals[i] = 0; // No action.
                }
            }
        }

        delete[] kmax; // Deallocate memory for the K-maximum array.
        delete[] kmin; // Deallocate memory for the K-minimum array.

        return signals; // Return the array of trading signals.
    }

    ~TrendFollowingStrategy()
    {
        strategyParams.clear();
        if (DEBUG)
            printMessage("Deconstructing Strategy Object");
    }
};

class Backtest
{
    Strategy *strategyInstance;
    std::shared_ptr<Data> dataInstance;
    int8_t *tradeSignals;

public:
    Backtest(Strategy *strategyInstance, std::shared_ptr<Data> dataInstance)
        : strategyInstance(strategyInstance), dataInstance(dataInstance), tradeSignals(nullptr)
    {
    }

    void printResults(const int &totalTrades, const int &numOfProfitableTrades, const float &totalProfitPercent)
    {
        stdOutMutex.lock();
        std::cout << "\n*************************************************************\n";
        std::cout << "Symbol: " << dataInstance->symbolName << std::endl;
        std::cout << "Strategy: " << strategyInstance->strategyName << std::endl;
        std::cout << "Total Trades Taken: " << totalTrades << std::endl;
        std::cout << "Number Of Profitable Trades: " << numOfProfitableTrades << std::endl;
        std::cout << "Total Profit Percentage: " << totalProfitPercent << std::endl;
        std::cout << "Average Profit Percentage Per Trade: " << float(totalProfitPercent) / float(totalTrades) << std::endl;
        std::cout << "*************************************************************\n";
        stdOutMutex.unlock();
    }

    // Function 'evaluateResults' calculates and prints trading strategy evaluation results.
    // It computes total profit percentage, the number of profitable trades, and total trades.

    void evaluateResults()
    {
        float openPrice;
        float totalProfitPercent = 0;
        int numOfProfitableTrades = 0, totalTrades = 0;

        for (int i = 0; i < dataInstance->numberOfRows; i++)
        {
            if (tradeSignals[i] == 1)
            {
                openPrice = dataInstance->symbolData[i].closePrice; // Record the opening price for a long position.
            }
            else if (tradeSignals[i] == -1)
            {
                // Calculate profit for exiting a long position.
                float profit = Strategy::findPercentageChange(openPrice, dataInstance->symbolData[i].closePrice);
                totalProfitPercent += profit; // Accumulate the total profit.
                totalTrades += 1;             // Increment the total trades count.
                if (profit > 0)
                    numOfProfitableTrades += 1; // Increment the profitable trades count if the trade was profitable.
            }
            else if (tradeSignals[i] == 2)
            {
                openPrice = dataInstance->symbolData[i].closePrice; // Record the opening price for a short position.
            }
            else if (tradeSignals[i] == -2)
            {
                // Calculate profit for exiting a short position.
                float profit = Strategy::findPercentageChange(dataInstance->symbolData[i].closePrice, openPrice);
                totalProfitPercent += profit; // Accumulate the total profit.
                totalTrades += 1;             // Increment the total trades count.
                if (profit > 0)
                    numOfProfitableTrades += 1; // Increment the profitable trades count if the trade was profitable.
            }
        }

        // Print the evaluation results.
        printResults(totalTrades, numOfProfitableTrades, totalProfitPercent);
    }

    void runBacktest()
    {
        tradeSignals = strategyInstance->getTradeSignals(*dataInstance);
        evaluateResults();
    }

    ~Backtest()
    {
        delete tradeSignals;
        if (DEBUG)
            printMessage("Deconstructing Backtest Object");
    }
};

class Driver
{
    int NUM_OF_THREADS;
    std::queue<std::pair<std::string, std::string>> symbolInputs;

public:
    std::mutex queueMutex;
    Strategy *strategyInstance;

    Driver()
        : NUM_OF_THREADS(5)
    {
    }

    Driver(int NUM_OF_THREADS)
        : NUM_OF_THREADS(NUM_OF_THREADS)
    {
    }

    void setStrategyInstance(Strategy *strategyInstance)
    {
        this->strategyInstance = strategyInstance;
    }

    void setSymbolInputs()
    {
        symbolInputs.push(std::make_pair("Meta", "data/META.csv"));
        symbolInputs.push(std::make_pair("Tesla", "data/TSLA.csv"));
        symbolInputs.push(std::make_pair("Amazon", "data/AMZN.csv"));
        symbolInputs.push(std::make_pair("Apple", "data/AAPL.csv"));
        symbolInputs.push(std::make_pair("Google", "data/GOOG.csv"));
    }

    void setSymbolInputs(std::queue<std::pair<std::string, std::string>> &symbolInputs)
    {
        this->symbolInputs = symbolInputs;
    }
    // Function 'processSymbol' handles the processing of a single symbol for backtesting.
    // It creates a shared pointer to Data, initializes a Backtest instance, and runs the backtest.
    // Finally, it deletes the Backtest instance when done.

    static void processSymbol(const std::pair<std::string, std::string> &symbolInput, Strategy *strategyInstance)
    {
        if (DEBUG)
            printMessage("Thread Started For Symbol: " + symbolInput.first);

        // Create a shared pointer to Data for the symbol.
        std::shared_ptr<Data> symbolData = std::make_shared<Data>(symbolInput.first, symbolInput.second);

        // Initialize a Backtest instance with the provided strategy and symbol data.
        Backtest *backtestInstance = new Backtest(strategyInstance, symbolData);

        // Run the backtest.
        backtestInstance->runBacktest();

        if (DEBUG)
            printMessage("Thread Completed For Symbol: " + symbolInput.first);

        // Delete the Backtest instance to free up resources.
        delete backtestInstance;
    }

    // Function 'runThreadTask' is a thread task function that processes symbols from a queue.
    // It continuously dequeues symbol inputs, processes them using 'processSymbol', and repeats until the queue is empty.

    static void runThreadTask(Driver *driverInstance)
    {
        while (driverInstance->symbolInputs.size())
        {
            driverInstance->queueMutex.lock();

            if (driverInstance->symbolInputs.size())
            {
                // Dequeue a symbol input and unlock the mutex.
                std::pair<std::string, std::string> symbolInput = driverInstance->symbolInputs.front();
                driverInstance->symbolInputs.pop();
                driverInstance->queueMutex.unlock();

                // Process the symbol using 'processSymbol'.
                processSymbol(symbolInput, driverInstance->strategyInstance);
            }
            else
            {
                driverInstance->queueMutex.unlock();
            }
        }
    }

    // Function 'runBacktest' initiates and manages parallel backtesting using multiple threads.
    // It creates an array of shared pointers to threads, each running the 'runThreadTask' function.
    // The function then waits for all threads to complete using 'join' before returning.

    void runBacktest()
    {
        std::shared_ptr<std::thread> threadsArray[NUM_OF_THREADS];

        // Create and start multiple threads, each running 'runThreadTask'.
        for (int i = 0; i < NUM_OF_THREADS; i++)
        {
            std::shared_ptr<std::thread> threadInstance = std::make_shared<std::thread>(Driver::runThreadTask, this);
            threadsArray[i] = threadInstance;
        }

        // Wait for all threads to complete.
        for (int i = 0; i < NUM_OF_THREADS; i++)
        {
            threadsArray[i]->join();
        }
    }

    ~Driver()
    {
        if (DEBUG)
            printMessage("Deconstructing Driver Object");
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