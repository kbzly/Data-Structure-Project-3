// Project identifier: 292F24D17A4455C1B5133EDD8C7CEAA0C9570A98
#include <iostream>
#include <queue>
#include <vector>
#include <getopt.h>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <fstream>
#include <sstream>
#include <algorithm>

using namespace std;

class User {
public:
    unsigned int balance;
    string pin;
    uint64_t reg_timestamp;
    unordered_set<string> activeSession;
    vector<unsigned int> incoming; // store index in transactions when self as recipient
    vector<unsigned int> outcoming; // store index in transactions when self as sende 
    User() = default;
    User(unsigned int bal, string p, uint64_t reg_time) 
        : balance(bal), pin(p), reg_timestamp(reg_time) {}
    
    void Login(string USER_ID, string IP, bool verbose_was_set) {
        activeSession.insert(IP);
        if (verbose_was_set)
        {
            cout << "User " << USER_ID << " logged in.\n";
        }   
    }

    void Logout(string USER_ID, string IP, bool verbose_was_set) {
        if (activeSession.find(IP) != activeSession.end())
        {   
            activeSession.erase(IP);
            if (verbose_was_set)
            {
                cout << "User " << USER_ID << " logged out.\n";
            }   
        } else {
            if (verbose_was_set)
            {
                cout << "Logout failed for " << USER_ID << ".\n";
            }   
        }
    }

    void Balance(string ACCOUNT, string IP, bool verbose_was_set, bool recentPlace_was_set, uint64_t recentPlace_timestamp) {
        if (activeSession.empty()) {
            if (verbose_was_set)
            {
                cout << "Account " << ACCOUNT << " is not logged in.\n";
            }
        } else if (activeSession.find(IP) != activeSession.end()) {
            uint64_t balance_timestamp;
            if (recentPlace_was_set)
            {
                balance_timestamp = recentPlace_timestamp;
            } else {
                balance_timestamp = reg_timestamp;
            }
            cout << "As of " << balance_timestamp << ", " << ACCOUNT << " has a balance of $" << balance << ".\n";
        } else {
            if (verbose_was_set)
            {
                cout << "Fraudulent transaction detected, aborting request.\n";
            }
        }
        
    }

};

struct Transaction
{
    uint64_t executeDate;
    unsigned int transactionID;
    unsigned int amount;
    string sender;
    string recipient;
    string feeMode;
    unsigned bankfee;
    Transaction(uint64_t execDate, unsigned int transID, unsigned int amt, 
                const std::string& sndr, const std::string& rcpt, const std::string& fee)
        : executeDate(execDate), transactionID(transID), amount(amt), 
          sender(sndr), recipient(rcpt), feeMode(fee) {}
};

struct SortByExecuteDate 
{
    bool operator()(const Transaction &left, const Transaction &right) const {
        if (left.executeDate == right.executeDate) {
            return left.transactionID > right.transactionID;
        }
        return left.executeDate > right.executeDate;
    }
};

bool compareExecuteDate(const Transaction& transaction, uint64_t date) {
    return transaction.executeDate < date;
}

static option long_options[] = {
    {"file",      required_argument, nullptr, 'f'},
    {"verbose",   no_argument,       nullptr, 'v'},
    {"help",      no_argument,       nullptr, 'h'},
    {nullptr,      0,                 nullptr,  0}
};

string get_optarg_argument_as_string() {
    if (optarg == nullptr) {
        throw "required argument is missing";
    }
    std::string str(optarg); // convert from const char* to std::string.
    return str;
}

// Common function
void readUser(const string filename, unordered_map<string, User>& users);
uint64_t convertTimeStamp(string timestamp);
unsigned calculateBankFee(unsigned int amount, string feeMode, unordered_map<std::string, User>::iterator Iter, u_int64_t execute_ts, bool isSender);
pair<vector<Transaction>::const_iterator, vector<Transaction>::const_iterator> findTransactionsInRange(const vector<Transaction>& transactions, uint64_t x, uint64_t y);
string timeIntervaltoFormat(u_int64_t timeInterval);

// Function for User Commands
void updateToCurrentTimeStamp(unordered_map<string, User>& users, priority_queue<Transaction, vector<Transaction>, SortByExecuteDate> &unexecutedTransactions, 
            vector<Transaction> &transactions, unsigned &transactionIDSuccessed, bool& verbose_was_set, uint64_t place_timestamp);
void login(stringstream& ss, unordered_map<string, User>& users, bool& verbose_was_set);
void out(stringstream& ss, unordered_map<string, User>& users, bool& verbose_was_set);
void balance(stringstream& ss, unordered_map<string, User>& users, bool& verbose_was_set, bool& recentPlace_was_set, uint64_t& recentPlace_timestamp);
void place(stringstream& ss, unordered_map<string, User>& users, bool& verbose_was_set, bool& recentPlace_was_set, uint64_t& recentPlace_timestamp, 
            priority_queue<Transaction, vector<Transaction>, SortByExecuteDate> &unexecutedTransactions, vector<Transaction> &transactions, unsigned &transactionIDSuccessed, unsigned int& transactionID);

// Fundtion for Query list
void ListTransactions(stringstream& ss, vector<Transaction> &transactions);
void BankRevenue(stringstream& ss, vector<Transaction> &transactions);
void CustomerHistory(stringstream& ss, vector<Transaction> &transactions, unordered_map<string, User>& users);
void SummarizeDay(stringstream& ss, vector<Transaction> &transactions);


int main(int argc, char** argv) {
    std::ios_base::sync_with_stdio(false);
    try
    {
        string filename;

        bool file_was_set = false;
        bool verbose_was_set = false;


        int choice = 0;
        while ((choice = getopt_long(argc, argv, "f:vh", long_options, nullptr)) != -1) {
            switch (choice) {
            case 'f':
                file_was_set = true;
                filename = get_optarg_argument_as_string();
                break;
            case 'v':
                verbose_was_set = true;
                break;
            case 'h':
                std::cout <<
                " --file/-f filename\n"
                "         This is followed by a filename for the registration file.\n"
                " --verbose/-v\n"
                "         This causes the program to print certain log messages, as defined in the spec.\n"
                " --help or -h\n"
                "         Prints this input specification.\n";
                return 0; // return from main with success
            default:
                // unrecognized option
                throw "unrecognized option";
            }
        }

        if (!file_was_set)
        {
            throw "Program should receive a --file/-f option, followed by the name of the account registration file\n";
        }

        unordered_map<string, User> users;
        
        // read information from filename
        readUser(filename, users);

        priority_queue<Transaction, vector<Transaction>, SortByExecuteDate> unexecutedTransactions;
        vector<Transaction> transactions;
        unsigned int transactionID = 0;
        unsigned int transactionIDSuccessed = 0;
        bool recentPlace_was_set = false;
        uint64_t recentPlace_timestamp = UINT64_MAX;
    

        string line;
        // User commands
        while (getline(cin, line))
        {
            if (!line.empty() && line[0] == '#') continue;

            if (line == "$$$") {
                updateToCurrentTimeStamp(users, unexecutedTransactions, transactions, transactionIDSuccessed, verbose_was_set, UINT64_MAX);
                break;
            }

            stringstream ss(line);
            string command;
            ss >> command;
            if (command == "place") {   
                place(ss, users, verbose_was_set, recentPlace_was_set, recentPlace_timestamp, unexecutedTransactions, transactions, transactionIDSuccessed, transactionID);
            } else if (command == "login") {
                login(ss, users, verbose_was_set);
            } else if (command == "out") {
                out(ss, users, verbose_was_set);
            } else { // command == "balance"
                balance(ss, users, verbose_was_set,recentPlace_was_set, recentPlace_timestamp);
            }
        }

        // Query List
        while (getline(cin, line))
        {
            stringstream ss(line);
            string command;
            ss >> command;
            if (command == "l") {
                ListTransactions(ss, transactions);
            } else if (command == "r") {
                BankRevenue(ss, transactions);
            } else if (command == "h") {
                CustomerHistory(ss, transactions, users);
            } else if (command == "s") {
                SummarizeDay(ss, transactions);
            }
        }   
    }
    catch(const char* err)
    {
        std::cerr << err << std::endl; // print the error message to std::cerr
        return 1; // returning 1 from main indicates that an error occurred
    }
    
}

void readUser(const string filename, unordered_map<string, User>& users){
    ifstream regFile(filename);

    if (!regFile.is_open()) {
        throw 
        "Error: Reading from cin has failed\n"
        "Registration file failed to open.\n";
    }

    string timestamp, name, pin, balanceStr;
    string line;
    while (getline(regFile, line)) {
        stringstream ss(line);

        // Reading data assuming format: timestamp|name|Pin|balance
        getline(ss, timestamp, '|');
        getline(ss, name, '|');
        getline(ss, pin, '|');
        ss >> balanceStr;

        unsigned int balance = stoi(balanceStr);

        // Insert user into the map
        users[name] = User(balance, pin, convertTimeStamp(timestamp));
    }
}

uint64_t convertTimeStamp(string timestamp){
    uint64_t ans = 0;
    ans += (timestamp[0] - '0') * 100000000000ULL;
    ans += (timestamp[1] - '0') * 10000000000ULL;
    ans += (timestamp[3] - '0') * 1000000000ULL;
    ans += (timestamp[4] - '0') * 100000000ULL;
    ans += (timestamp[6] - '0') * 10000000ULL;
    ans += (timestamp[7] - '0') * 1000000ULL;
    ans += (timestamp[9] - '0') * 100000ULL;
    ans += (timestamp[10] - '0') * 10000ULL;
    ans += (timestamp[12] - '0') * 1000ULL;
    ans += (timestamp[13] - '0') * 100ULL;
    ans += (timestamp[15] - '0') * 10ULL;
    ans += (timestamp[16] - '0') * 1ULL;

    return ans;
}

unsigned calculateBankFee(unsigned int amount, string feeMode, unordered_map<std::string, User>::iterator SenderIter, u_int64_t execute_ts, bool isSender) {
    unsigned int ans = amount / 100;
    ans = max(10u, ans);
    ans = min(450u, ans);

    // Then find if discount considered
    if (execute_ts - SenderIter->second.reg_timestamp > 50000000000ULL)
    {
        ans = (ans * 3) / 4;
    }

    if (!isSender && feeMode == "o") {
        ans = 0;
    } else if (feeMode == "s") {
        ans = (ans % 2 == 0) ? ans / 2 : (ans / 2) + (isSender ? 1 : 0);
    } 
    
    return ans;
}

pair<vector<Transaction>::const_iterator, vector<Transaction>::const_iterator>
findTransactionsInRange(const vector<Transaction>& transactions, uint64_t x, uint64_t y) {
    auto startIter = lower_bound(transactions.begin(), transactions.end(), x, compareExecuteDate);
    // the valid range should be endIt--, endIt is just fine to using end();
    auto endIter = lower_bound(transactions.begin(), transactions.end(), y, compareExecuteDate);

    return {startIter, endIter};
}

string timeIntervaltoFormat(u_int64_t timeInterval) {
    string ans;
    vector<u_int64_t> num = {0, 0, 0, 0, 0, 0};
    num[0] = timeInterval / 10000000000ULL;
    timeInterval %= 10000000000ULL;
    num[1] = timeInterval / 100000000ULL;
    timeInterval %= 100000000ULL;
    num[2] = timeInterval / 1000000ULL;
    timeInterval %= 1000000ULL;
    num[3] = timeInterval / 10000ULL;
    timeInterval %= 10000ULL;
    num[4] = timeInterval / 100ULL;
    timeInterval %= 100ULL;
    num[5] = timeInterval / 1ULL;
    vector<string> str = {((num[0] != 1) ? "years" : "year"), ((num[1] != 1) ? "months" : "month"), ((num[2] != 1) ? "days" : "day"), ((num[3] != 1) ? "hours" : "hour"), ((num[4] != 1) ? "minutes" : "minute"), ((num[5] != 1) ? "seconds" : "second")};

    for (size_t i = 0; i < 6; i++)
    {   
        if (num[i] == 0) continue;
        ans += " " + to_string(num[i]) + " " + str[i];
    }
    return ans;    
}

void updateToCurrentTimeStamp(unordered_map<string, User>& users, priority_queue<Transaction, vector<Transaction>, SortByExecuteDate> &unexecutedTransactions, vector<Transaction> &transactions,
             unsigned &transactionIDSuccessed, bool& verbose_was_set, uint64_t place_timestamp) {
    while (!unexecutedTransactions.empty() && place_timestamp >= unexecutedTransactions.top().executeDate)
        {   
            Transaction currentExecute = unexecutedTransactions.top(); unexecutedTransactions.pop();
            auto sIter = users.find(currentExecute.sender);
            auto rIter = users.find(currentExecute.recipient);

            // find bank fee
            unsigned int sfee = calculateBankFee(currentExecute.amount, currentExecute.feeMode, sIter, currentExecute.executeDate, true);
            unsigned int rfee = calculateBankFee(currentExecute.amount, currentExecute.feeMode, sIter, currentExecute.executeDate, false);

            // whether deal is maked
            if (sIter->second.balance >= currentExecute.amount + sfee && rIter->second.balance >= rfee)
            {
                // transaction success
                
                // update balance
                sIter->second.balance -= currentExecute.amount + sfee;
                rIter->second.balance += currentExecute.amount - rfee;
                // add index of transaction for Customer History using
                sIter->second.outcoming.push_back(transactionIDSuccessed);
                rIter->second.incoming.push_back(transactionIDSuccessed);
                transactionIDSuccessed++;
                // add transactions
                currentExecute.bankfee = sfee + rfee;
                transactions.push_back(currentExecute);
                if (verbose_was_set) cout << "Transaction " << currentExecute.transactionID << " executed at " << currentExecute.executeDate << ": $" << currentExecute.amount << " from " << currentExecute.sender << " to " << currentExecute.recipient << ".\n";
            } else {
                if (verbose_was_set) cout << "Insufficient funds to process transaction " << currentExecute.transactionID <<".\n";
            }
        }
}

void login(stringstream& ss, unordered_map<string, User>& users, bool& verbose_was_set) {
    string USER_ID, PIN, IP;
    ss >> USER_ID >> PIN >> IP;

    if (auto iter = users.find(USER_ID); iter != users.end() && iter->second.pin == PIN)
    {
        iter->second.Login(USER_ID, IP, verbose_was_set);
    } else {
        if (verbose_was_set)
        {
            cout << "Login failed for " << USER_ID << ".\n";
        }
    }
}

void out(stringstream& ss, unordered_map<string, User>& users, bool& verbose_was_set) {
    string USER_ID, IP;
    ss >> USER_ID >> IP;

    if (auto iter = users.find(USER_ID); iter != users.end())
    {
        iter->second.Logout(USER_ID, IP, verbose_was_set);
    } else {
        if (verbose_was_set)
        {
            cout << "Logout failed for " << USER_ID << ".\n";
        }
    }
}

void balance(stringstream& ss, unordered_map<string, User>& users, bool& verbose_was_set, bool& recentPlace_was_set, uint64_t& recentPlace_timestamp) {
    string ACCOUNT, IP;
    ss >> ACCOUNT >> IP;

    if (auto iter = users.find(ACCOUNT); iter != users.end())
    {
        iter->second.Balance(ACCOUNT, IP, verbose_was_set, recentPlace_was_set, recentPlace_timestamp);
    } else {
        if (verbose_was_set)
        {
            cout << "Account " << ACCOUNT << " does not exist.\n";
        }
    }
}

void place(stringstream& ss, unordered_map<string, User>& users, bool& verbose_was_set, bool& recentPlace_was_set, uint64_t& recentPlace_timestamp, 
            priority_queue<Transaction, vector<Transaction>, SortByExecuteDate> &unexecutedTransactions, vector<Transaction> &transactions, unsigned &transactionIDSuccessed, unsigned int& transactionID) {
    string timestampStr, IP, SENDER, RECIPIENT, AMOUNTStr, EXEC_DATEStr, feeMode;
    ss >> timestampStr >> IP >> SENDER>> RECIPIENT >> AMOUNTStr >> EXEC_DATEStr >> feeMode;

    uint64_t place_timestamp = convertTimeStamp(timestampStr);
    uint64_t execute_timestamp = convertTimeStamp(EXEC_DATEStr);


    // check A place command with a timestamp earlier than the previous place.
    if (recentPlace_was_set && place_timestamp < recentPlace_timestamp)
    {
        throw "Invalid decreasing timestamp in 'place' command.\n";
    }
    // check A place command which contains an execution date before its own timestamp. 
    if (place_timestamp > execute_timestamp)
    {
        throw "You cannot have an execution date before the current timestamp.\n";
    }
    
    // update recent ts for Balance using
    if (!recentPlace_was_set) recentPlace_was_set = true;
    recentPlace_timestamp = place_timestamp;
    
    // check The sender is different from the recipient
    if (SENDER == RECIPIENT)
    {
        if (verbose_was_set) cout << "Self transactions are not allowed.\n";
        return;
    }

    // check An execution date that’s three or less days from the timestamp of the transaction
    if (execute_timestamp - place_timestamp > 3000000ULL)
    {
        if (verbose_was_set) cout << "Select a time up to three days in the future.\n";
        return;
    }

    // check The sender exists (in the registration data)
    auto senderIter = users.find(SENDER);
    if (senderIter == users.end())
    {
        if (verbose_was_set) cout << "Sender " << SENDER << " does not exist.\n";
        return;
    }

    // check The recipient exists
    auto recipientIter = users.find(RECIPIENT);
    if (recipientIter == users.end())
    {
        if (verbose_was_set) cout << "Recipient " << RECIPIENT << " does not exist.\n";
        return;
    }

    // check An execution date that is later than the sender’s and recipient’s registration date (both users must have accounts already created at the execution time of the transaction)
    if (senderIter->second.reg_timestamp > execute_timestamp || recipientIter->second.reg_timestamp > execute_timestamp)
    {
        if (verbose_was_set) cout << "At the time of execution, sender and/or recipient have not registered.\n";
        return;
    }

    // check An active user session (i.e., the sender must be logged in, at the IP address given in the command)
    if (senderIter->second.activeSession.empty())
    {
        if (verbose_was_set) cout << "Sender " << SENDER << " is not logged in.\n";
        return;
    }

    // check An active user session (i.e., the sender must be logged in, at the IP address given in the command)
    if (senderIter->second.activeSession.find(IP) == senderIter->second.activeSession.end())
    {
        if (verbose_was_set) cout << "Fraudulent transaction detected, aborting request.\n";
        return;
    } else {
        // execute all transaction earlier than place_timestamp
        updateToCurrentTimeStamp(users, unexecutedTransactions, transactions, transactionIDSuccessed, verbose_was_set, place_timestamp);
        // add Transaction to unexecutedTransactions
        unexecutedTransactions.push(Transaction(execute_timestamp, transactionID, stoi(AMOUNTStr), SENDER, RECIPIENT, feeMode));                   
        if (verbose_was_set) cout << "Transaction " << transactionID << " placed at " <<  place_timestamp << ": $" << AMOUNTStr << " from " << SENDER << " to " << RECIPIENT << " at " << execute_timestamp << ".\n";
        transactionID++;
    } 
}

void ListTransactions(stringstream& ss, vector<Transaction> &transactions) {
    unsigned int transactionCount = 0;
    string x, y;
    ss >> x >> y;
    u_int64_t _x = convertTimeStamp(x);
    u_int64_t _y = convertTimeStamp(y);
    u_int64_t timeInterval = _y - _x;
    if (timeInterval == 0)
    {
        cout << "List Transactions requires a non-empty time interval.\n";
        return;
    }

    auto range = findTransactionsInRange(transactions, _x, _y);
    for (auto it = range.first; it != range.second; ++it) {
        cout << it->transactionID << ": " << it->sender << " sent " << it->amount << " " << ((it->amount != 1) ? "dollars" : "dollar") << " to " << it->recipient << " at " << it->executeDate << ".\n";
        transactionCount++;
    }
    cout << "There " << ((transactionCount != 1) ? "were" : "was") << " " << transactionCount << " " << ((transactionCount != 1) ? "transactions" : "transaction") << " that " << ((transactionCount != 1) ? "were" : "was") << " placed between time " << _x << " to " << _y << ".\n";
}

void BankRevenue(stringstream& ss, vector<Transaction> &transactions) {
    unsigned int bankRevenue = 0;
    string x, y;
    ss >> x >> y;
    u_int64_t _x = convertTimeStamp(x);
    u_int64_t _y = convertTimeStamp(y);
    u_int64_t timeInterval = _y - _x;
    if (timeInterval == 0)
    {
        cout << "Bank Revenue requires a non-empty time interval.\n";
        return;
    }
    
    auto range = findTransactionsInRange(transactions, _x, _y);
    for (auto it = range.first; it != range.second; ++it) {
        bankRevenue += it->bankfee;
    }
    cout << "281Bank has collected " << bankRevenue << " dollars in fees over" << timeIntervaltoFormat(timeInterval) << ".\n";
}

void CustomerHistory(stringstream& ss, vector<Transaction> &transactions, unordered_map<string, User>& users) {
    string user_id;
    ss >> user_id;
    if (auto iter = users.find(user_id); iter == users.end())
    {
        cout << "User " << user_id << " does not exist.\n";
        return;
    }
    cout << "Customer " << user_id << " account summary:\n";
    User customer = users[user_id];
    cout << "Balance: $" << customer.balance << "\n";
    cout << "Total # of transactions: " << customer.incoming.size() + customer.outcoming.size() << "\n";

    size_t incomingSize = customer.incoming.size();
    size_t incomingStart = (incomingSize > 10) ? incomingSize - 10 : 0;
    cout << "Incoming " << incomingSize << ":\n";
    for (size_t i = incomingStart; i < incomingSize; i++) 
    {   
        const Transaction& tempTrans = transactions[customer.incoming[i]];
        cout << tempTrans.transactionID << ": " << tempTrans.sender << " sent " << tempTrans.amount << " " << ((tempTrans.amount != 1) ? "dollars" : "dollar") << " to " << tempTrans.recipient << " at " << tempTrans.executeDate << ".\n";
    }

    size_t outgoingSize = customer.outcoming.size();
    size_t outgoingStart = (outgoingSize > 10) ? outgoingSize - 10 : 0;
    cout << "Outgoing " << outgoingSize << ":\n";
    for (size_t i = outgoingStart; i < outgoingSize; i++) {
        const Transaction& tempTrans = transactions[customer.outcoming[i]];
        cout << tempTrans.transactionID << ": " << tempTrans.sender << " sent " << tempTrans.amount << " " << ((tempTrans.amount != 1) ? "dollars" : "dollar") << " to " << tempTrans.recipient << " at " << tempTrans.executeDate << ".\n";
    }
}

void SummarizeDay(stringstream& ss, vector<Transaction> &transactions) {
    unsigned int transactionCount = 0;
    unsigned int bankRevenue = 0;
    string x;
    ss >> x;
    u_int64_t _x = convertTimeStamp(x);

    // find the day timestamp
    _x = (_x / 1000000ULL) * 1000000ULL;
    u_int64_t _y = _x + 1000000ULL;
    cout << "Summary of [" << _x << ", " << _y << "):\n";

    auto range = findTransactionsInRange(transactions, _x, _y);
    for (auto it = range.first; it != range.second; ++it) {
        cout << it->transactionID << ": " << it->sender << " sent " << it->amount << " " << ((it->amount != 1) ? "dollars" : "dollar") << " to " << it->recipient << " at " << it->executeDate << ".\n";
        transactionCount++;
        bankRevenue += it->bankfee;
    }
    cout << "There " << ((transactionCount != 1) ? "were" : "was") << " a total of " << transactionCount << " " << ((transactionCount != 1) ? "transactions" : "transaction") << ", 281Bank has collected " << bankRevenue << " dollars in fees.\n";
}
    

// string convertTimeStamptoString(uint64_t timestamp){
//     string ans;
//     ans += (timestamp / 100000000000ULL) + '0';
//     timestamp %= 100000000000ULL;
//     ans += (timestamp / 10000000000ULL) + '0';
//     timestamp %= 10000000000ULL;
//     ans += ':';
//     ans += (timestamp / 1000000000ULL) + '0';
//     timestamp %= 1000000000ULL;
//     ans += (timestamp / 100000000ULL) + '0';
//     timestamp %= 100000000ULL;
//     ans += ':';
//     ans += (timestamp / 10000000ULL) + '0';
//     timestamp %= 10000000ULL;
//     ans += (timestamp / 1000000ULL) + '0';
//     timestamp %= 1000000ULL;
//     ans += ':';
//     ans += (timestamp / 100000ULL) + '0';
//     timestamp %= 100000ULL;
//     ans += (timestamp / 10000ULL) + '0';
//     timestamp %= 10000ULL;
//     ans += ':';
//     ans += (timestamp / 1000ULL) + '0';
//     timestamp %= 1000ULL;
//     ans += (timestamp / 100ULL) + '0';
//     timestamp %= 100ULL;
//     ans += ':';
//     ans += (timestamp / 10ULL) + '0';
//     timestamp %= 10ULL;
//     ans += (timestamp / 1ULL) + '0';
//     return ans;
// }
