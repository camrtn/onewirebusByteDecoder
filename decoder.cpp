#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <limits>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

struct Sample {
    double time;
    double voltage;
};

// Parameters
const double LOW_THRESHOLD = 1.5;     // Voltage below this is LOW

const double M_SHORT_PULSE_US = 2.0;
const double M_LONG_PULSE_US = 9.0;

const double S_LONG_PULSE_US = 2.5;   // Slave sends 0 with ~4μs pulse, some anomalies where it's around 1.5-2uS
                                      // if pulse is shorter than ~1.5uS the slave sent a '1'

const double BURST_GAP_THRESHOLD_US = 100.0; // Idle time to detect new burst

int main() {
    int selection = 0; //.csv file selection from user

    bool mode = 0; //flag to control if master or slave data is being decoded
    bool debug = 0;  // Flag to control printing of bytes per burst

    std::string userInput;
    std::string csv;
    std::string ext = ".csv";
    std::string directory = "../DATA/";
    std::vector<fs::path> csvFiles;

    if(!fs::exists(directory))
    {
        std::cerr << "Directory does not exist: " << directory << std::endl;
    }

    std::cout << "-------------------------------------------" << std::endl;

    // List .csv files and number them
    int index = 1;
    for (const auto& entry : fs::directory_iterator(directory))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".csv")
        {
           csvFiles.push_back(entry.path());
            std::cout << index++ << ". " << entry.path().filename().string() << std::endl;
        }
    }
    std::cout << "-------------------------------------------" << std::endl;

    if (csvFiles.empty()) {
        std::cout << "No .csv files found in directory." << std::endl;
        return 1;
    }

    std::cout << std::endl;
    std::cout <<"Choose the file you want to decode: " << std::flush;
    std::cin >> selection;

    //clear terminal up to the beginning of the field that shows .csv files
    for(int i = 0; i < index+3; i++)
    {
        std::cout << "\033[A\33[2K\r" << std::flush; //move cursor up one line, clear that line, return cursor to beginning
    }
    std::cout << std::endl; //insert newline under terminal prompt for formatting

    // Validate input
    if (selection < 1 || selection > static_cast<int>(csvFiles.size()))
    {
        std::cout << "\033[A\33[2K\r" << std::flush;
        std::cerr << "Invalid entry!" << std::endl;
        return 1;
    }

    // Get selected file path
    fs::path selectedFile = csvFiles[selection - 1];

    std::cout << "Decode (m)aster or (s)lave data? " << std::flush;
    std::cin >> userInput;
    std::cout << "\033[A\33[2K\r" << std::flush;

    if(userInput == "m")
    {
        mode = 1;
    }
    else if(userInput == "s")
    {
        mode = 0;
    }
    else
    {
        std::cout << "\033[A\33[2K\r" << std::flush;
        std::cerr << "Invalid entry!" << std::endl;
        return 1;
    }

    userInput = "0";

    std::cout << "Enable (d)ebug mode or (n)ormal mode? " << std::flush;
    std::cin >> userInput;
    std::cout << "\033[A\33[2K\r" << std::flush;
    std::cout << "\033[A\33[2K\r" << std::flush; //added second line clear for formatting sake
    
    if(userInput == "d")
    {
        debug = 1;
    }
    else if(userInput == "n")
    {
        debug = 0;
    }
    else
    {
        std::cerr << "Invalid entry!" << std::endl;
        return 1;
    }

    std::ifstream file(selectedFile);
    if (!file.is_open()) {
        std::cerr << "Failed to open file: " << directory << std::endl;
        return 1;
    }

    std::string line;
    std::vector<Sample> samples;
    std::getline(file, line); // Skip header
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string timeStr, voltStr;
        if (std::getline(ss, timeStr, ',') && std::getline(ss, voltStr)) {
            Sample s;
            s.time = std::stod(timeStr);
            s.voltage = std::stod(voltStr);
            samples.push_back(s);
        }
    }

    // Detect pulses from .csv data
    std::vector<double> pulseStartTimes;
    std::vector<double> pulseDurations;
    bool inLow = false;
    double lowStartTime = 0.0;

    for (size_t i = 1; i < samples.size(); ++i)
    {
        bool prevHigh = samples[i - 1].voltage >= LOW_THRESHOLD;
        bool currLow = samples[i].voltage < LOW_THRESHOLD;

        if (prevHigh && currLow)
        {
            inLow = true;
            lowStartTime = samples[i].time;
        }
        if (inLow && samples[i].voltage >= LOW_THRESHOLD)
        {
            double lowEndTime = samples[i].time;
            double duration = (lowEndTime - lowStartTime) * 1e6; // μs
            pulseStartTimes.push_back(lowStartTime);
            pulseDurations.push_back(duration);
            inLow = false;
        }
    }

    // Group pulses into data bursts
    std::vector<std::vector<double>> burstDurations;
    std::vector<double> currentBurst;

    for (size_t i = 0; i < pulseStartTimes.size(); ++i)
    {
        if (i == 0 || (pulseStartTimes[i] - pulseStartTimes[i - 1]) * 1e6 < BURST_GAP_THRESHOLD_US)
        {
            currentBurst.push_back(pulseDurations[i]);
        } 
        else
        {
            if (!currentBurst.empty())
            {
                burstDurations.push_back(currentBurst);
                currentBurst.clear();
            }
            currentBurst.push_back(pulseDurations[i]);
        }
    }

    if (!currentBurst.empty())
    {
        burstDurations.push_back(currentBurst);
    }

    std::vector<uint8_t> decodedBytes;

    // Decode each data burst
    for (size_t burstIndex = 0; burstIndex < burstDurations.size(); ++burstIndex) 
    {
        const auto& burst = burstDurations[burstIndex];
        std::vector<int> burstBits;
        int invalidBitCount = 0; //tracks how many invalid bits were present per data burst

        for (double dur : burst)
        {
            if (mode) //decode master data
            {
                if (dur < M_SHORT_PULSE_US)
                {
                    burstBits.push_back(1);
                }
                else if (dur > M_LONG_PULSE_US)
                {
                    burstBits.push_back(0);
                }
                else
                {
                    burstBits.push_back(-1);
                    invalidBitCount++;
                }
            }
            else //decode slave data
            {
                if (dur < S_LONG_PULSE_US)
                {
                    burstBits.push_back(1);
                }
                else if (dur >= S_LONG_PULSE_US)
                {
                    burstBits.push_back(0);
                }
                else
                {
                    burstBits.push_back(-1);
                    invalidBitCount++;
                }
            }
        }

        // Convert to bytes (LSB-first)
        std::vector<uint8_t> burstDecodedBytes;
        for (size_t i = 0; i + 7 < burstBits.size(); i += 8)
        {
            bool valid = true;
            uint8_t byte = 0;

            for (int j = 0; j < 8; ++j)
            {
                if (burstBits[i + j] == -1)
                {
                    valid = false;
                    break;
                }
                byte |= (burstBits[i + j] << j);
            }

            if (valid)
            {
                burstDecodedBytes.push_back(byte);
            }
        }

        // Output debug info about a burst
        if(debug)
        {
            std::cout << "Data burst " << burstIndex + 1 << " info: " << std::endl;

            //print out how many bytes were in the data burst, how many invalid bits were in the data burst, and the duration of each bit in the data burst
            std::cout << burstDecodedBytes.size() << " bytes" << std::endl;
            std::cout << invalidBitCount << " invalid bits" << std::endl;

            std::cout << "Bit durations (μs): ";
            for (double d : burst)
            {
                std::cout << d << " ";
            }
            std::cout << "\n\nData burst " << burstIndex + 1 << " bytes: ";
            std::cout << std::endl;
        }

        std::cout << std::endl;
        // Output decoded bytes
        for (size_t i = 0; i < burstDecodedBytes.size(); ++i)
        {
            std::cout << "0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(burstDecodedBytes[i]) << " ";
            if ((i + 1) % 8 == 0)
            {
                std::cout << "\n";
            }
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;

    return 0;
}