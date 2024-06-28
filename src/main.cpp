/*
    Nome - Matricula
    Pedro Henrique Rodrigues Bispo - 2020421857
    Lucas Gontijo - 2020077404
*/

#include <iostream>
#include <boost/asio.hpp>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <fstream>

using namespace std;

using boost::asio::ip::tcp;

#pragma pack(push, 1)

struct LogRecordData {
    char sensor_id[32];
    time_t timestamp;
    double value;
};
#pragma pack(pop)


std::time_t stringToTime(const std::string& time_string) {
    std::tm tm = {};
    std::istringstream ss(time_string);

    if (!(ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S"))) {
        throw std::runtime_error("Failed to parse time string: " + time_string);
    }

    tm.tm_isdst = -1;

    std::time_t time = std::mktime(&tm);
    if (time == -1) {
        throw std::runtime_error("Failed to convert to time_t: " + time_string);
    }

    return time;
}

std::string timeToString(std::time_t time) {
    std::ostringstream ss;
    std::tm* tm_ptr = std::localtime(&time);

    if (tm_ptr == nullptr) {
        throw std::runtime_error("Failed to convert time_t to tm struct.");
    }

    ss << std::put_time(tm_ptr, "%Y-%m-%dT%H:%M:%S");
    return ss.str();
}

void saveLogRecord(string filename, LogRecordData record_to_save) {
    fstream file(filename, fstream::out | fstream::in | fstream::binary | fstream::app);
    if (file.is_open()) {
        int filesize = file.tellg();
        int num_records = filesize / sizeof(LogRecordData);
        file.write((char*)&record_to_save, sizeof(LogRecordData));
        file.close();
    } else {
        cout << "Error opening file, try again" << endl;
    };
};


std::string retrieveSensorRecord(const std::string& filename, int numRecordsThatWillBeRetrieved) {
    std::ifstream file(filename, std::ios::in | std::ios::binary);

    if (!file.is_open()) {
        return "ERROR|INVALID_SENSOR_ID\r\n";
    }

    std::ostringstream data;
    data << numRecordsThatWillBeRetrieved;

    for (int i = 0; i < numRecordsThatWillBeRetrieved; ++i) {
        LogRecordData record;
        file.read(reinterpret_cast<char*>(&record), sizeof(LogRecordData));

        if (!file) {
            break;
        }

        if (record.value >= -0.000001 && record.value <= 0.00001) {
            return "ERROR|INVALID_SENSOR_ID\r\n";
        }

        data << ";" << timeToString(record.timestamp) << "|" << record.value;
    }

    data << "\r\n";
    return data.str();
}

vector<string> spintString(const string& input_string, char delimiter_char) {
    vector<string> substrings;
    string substring;
    for (char c : input_string) {
        if (c != delimiter_char) {
            substring += c;
        } else if (!substring.empty()) {
            substrings.push_back(substring);
            substring.clear();
        };
    };
    if (!substring.empty()) {
        substrings.push_back(substring);
    };
    return substrings;
};

class session : public enable_shared_from_this<session> {
public:
    session(tcp::socket socket) : socket_(move(socket)) {}
    void start() {
        readIncomingMessage();
    };
    
private:
    void readIncomingMessage() {
        auto self(shared_from_this());
        boost::asio::async_read_until(socket_, buffer_, "\r\n",
            [this, self](boost::system::error_code error, size_t length) {
                if (!error) {
                    istream is(&buffer_);
                    string message(istreambuf_iterator<char>(is), {});
                    string reply_msg;
                    cout << message << endl;
                    vector<string> data = spintString(message, '|');
                    if (data[0] == "LOG") {
                        LogRecordData log;
                        strcpy(log.sensor_id, data[1].c_str());
                        log.timestamp = stringToTime(data[2]);
                        log.value = stod(data[3]);              
                        string filename = data[1] + ".dat";
                        saveLogRecord(filename, log);
                        writeMessage(reply_msg);
                    } else if(data[0] == "GET") {
                        int num_reg = stoi(data[2]);
                        string filename = data[1] + ".dat";
                        reply_msg = retrieveSensorRecord(filename, num_reg);
                        writeMessage(reply_msg);
                    };
                };
            }
        );
    };
    void writeMessage(const string& msg) {
        auto self(shared_from_this());
        boost::asio::async_write(socket_, boost::asio::buffer(msg),
            [this, self, msg](boost::system::error_code error, size_t length) {
                if (!error) {
                    readIncomingMessage();
                };
            }
        );
    };
    tcp::socket socket_;
    boost::asio::streambuf buffer_;
};

class server {
public:
    server(boost::asio::io_context& io_context, short port)
        : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)) {
        acceptConnection();
    };

private:
    void acceptConnection() {
        acceptor_.async_accept(
            [this](boost::system::error_code error, tcp::socket socket) {
                if (!error) {
                    make_shared<session>(move(socket))->start();
                };
                acceptConnection();
            }
        );
    };
    tcp::acceptor acceptor_;
};

int main(int argc, char* argv[]) {
    boost::asio::io_context io_context;
    server s(io_context, 9000);
    io_context.run();
    return 0;
};