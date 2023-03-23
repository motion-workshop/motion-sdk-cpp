// Copyright Motion Workshop. All Rights Reserved.
#include <Client.hpp>
#include <Format.hpp>

#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>

/*
  Example application for the Motion C++ SDK classes. Runs as a command line
  utility. Live stream data to a CSV file format. Default is to stream forever
  and write CSV formatted data to the standard output (std::cout). The user may
  specify an output file, a number of frames, and whether to print out the
  channel name header in the first row.

  Usage:
    ./example_MotionSDK
    ./example_MotionSDK --file out.csv --frames 25 --header
*/

/**
  Utility class to store all of the options to run our Motion SDK data stream.
*/
class command_line_options {
public:
    command_line_options();

    /**
      Read the command line tokens and load them into this state. Returns 0 if
      successful, -1 if the command line options are invalid, or 1 if the help
      message should be printed out.
    */
    int parse(int argc, char** argv);

    /**
      Print command line usage help for this program. Returns 1 which is
      intended to be the main return code as well.
    */
    int print_help(std::ostream* out, char* program_name);

    /**
      Store an error or informational message about why the parse phase failed.
      This will be shown to the user with additional help so they can correct
      the input parameters.
    */
    std::string message;

    /**
      Stream the formatted data to a file, defaults to empty which writes to
      std::cout.
    */
    std::string filename;

    /**
      Read N frames and then stop sampling, defaults to 0 which indicates no
      limit and to keep streaming for as long as possible.
    */
    int frames;

    /** IP address to connect to, defaults to "127.0.0.1". */
    std::string address;

    /** Port to connect to, defaults to 32076. */
    unsigned port;

    /** Print this string in between every column, defaults to ",". */
    std::string separator;

    /** Print this string in between every row, defaults to "\n". */
    std::string newline;

    /**
      Set to true to print out string channel names in the 0th row, defaults to
      false.
    */
    bool header;
}; // class command_line_options

/**
  Convert the flat XML <node id="Hips" key="4" ... /> list into a map of
  4 => "Hips" values. Matches the key of the Format maps.
*/
bool parse_name_map(
    const std::string& xml_node_list,
    std::map<std::size_t, std::string>& name_map);

/**
  Connect to a Motion Service by IP address and port number. Defaults to
  loopback (127.0.0.1) address and port 32076 (which is the Configurable data
  service).

  Request a list of channels by sending an XML list. Read frames forever and
  write out one row in a CSV spreadsheet per frame.

  Frames are a fixed size, so one channel is one column and one frame is one
  row in the CSV format.
*/
int stream_data_to_csv(
    std::ostream* output, std::ostream* error,
    const command_line_options& options)
{
    using Motion::SDK::Client;
    using Motion::SDK::Format;

    // Open connection to the configurable data service.
    Client client(options.address, options.port);
    if (!client.isConnected()) {
        *error << "failed to connect to Motion Service on " << options.address
               << ":" << options.port;
        return -1;
    }

    {
        // Request the channels that we want from every connected device. The
        // full list is available here:
        //
        //   https://www.motionshadow.com/download/media/configurable.xml
        //
        // Select the local quaternion (Lq) and positional constraint (c)
        // channels here. 8 numbers per device per frame. Ask for inactive nodes
        // which are not necessarily attached to a sensor but are animated as
        // part of the Shadow skeleton.
        const std::string xml = "<?xml version=\"1.0\"?>"
                                "<configurable inactive=\"1\">"
                                "<Lq/>"
                                "<c/>"
                                "</configurable>";

        if (!client.writeData(Client::data_type(xml.begin(), xml.end()))) {
            *error << "failed to send channel list request to Configurable "
                      "service";
            return -1;
        }
    }

    // Wait up to 1 second for an incoming message. This indicates that the
    // stream is active. Also, the first message is always the name map that we
    // can use to print out string names.
    if (!client.waitForData(1)) {
        *error << "no active data stream available, giving up";
        return -1;
    }

    bool print_header = options.header;

    std::map<std::size_t, std::string> name_map;
    if (print_header) {
        std::string xml;
        if (client.getXMLString(xml)) {
            if (!parse_name_map(xml, name_map)) {
                *error << "failed to parse XML name map";
                return -1;
            }
        }
    }

    int num_frames = 0;
    for (;;) {
        // Read one frame of data from all connected devices.
        Client::data_type data;
        if (!client.readData(data)) {
            *error << "data stream interrupted or timed out";
            return -1;
        }

        auto list = Format::MakeList<Format::ConfigurableElement>(
            data.begin(), data.end());

        if (print_header) {
            const std::string ChannelName[8] = {"Lqw", "Lqx", "Lqy", "Lqz",
                                                "cw",  "cx",  "cy",  "cz"};

            bool have_output_line = false;
            for (const auto& item : list) {
                auto itr = name_map.find(item.key());
                if (itr == name_map.end()) {
                    *error << "device missing from name map, unable to print "
                              "header";
                    return -1;
                }

                if (item.size() != 8) {
                    *error << "expected 8 channels but found " << item.size()
                           << ", unable to print header";
                    return -1;
                }

                for (const auto& name : ChannelName) {
                    if (have_output_line) {
                        *output << options.separator;
                    } else {
                        have_output_line = true;
                    }

                    *output << itr->second << "." << name;
                }
            }

            if (!have_output_line) {
                *error << "unknown data format, unabled to print header";
                return -1;
            }

            *output << options.newline;

            print_header = false;
        }

        // Iterate through the entries, one per device.
        bool have_output_line = false;
        for (const auto& item : list) {
            // Iterate through the channels per device. From the channel list we
            // know that each node has 8 channels.
            // [Lqw, Lqx, Lqy, Lqz, cw, cx, cy, cz]
            // Lq is unit quaternion rotation in the skeletal joint coordinate
            // frame cw is unitless scalar, 0 is not constrained, 1 is fully
            // constrained cx, cy, cz are global position in centimeters
            for (std::size_t i = 0; i < item.size(); ++i) {
                if (have_output_line) {
                    *output << options.separator;
                } else {
                    have_output_line = true;
                }

                *output << item[i];
            }
        }

        if (!have_output_line) {
            *error << "unknown data format in stream";
            return -1;
        }

        *output << options.newline;

        if (options.frames > 0) {
            if (++num_frames >= options.frames) {
                break;
            }
        }
    }

    return 0;
}

bool parse_name_map(
    const std::string& xml_node_list,
    std::map<std::size_t, std::string>& name_map)
{
    // If you have an XML parser go ahead and use that instead. For this
    // example, we will use regular expressions to avoid a dependency on an
    // external library.
    std::regex re("<node id=\"([^\"]+)\" key=\"(\\d+)\"");

    auto itr =
        std::sregex_iterator(xml_node_list.begin(), xml_node_list.end(), re);

    bool result = false;
    for (; itr != std::sregex_iterator(); ++itr) {
        const int key = std::stoi((*itr)[2].str());
        if (name_map.emplace(key, (*itr)[1]).second) {
            result = true;
        }
    }

    return result;
}

int main(int argc, char** argv)
{
    command_line_options options;
    if (options.parse(argc, argv) != 0) {
        return options.print_help(&std::cerr, *argv);
    }

    // Stream frames to a CSV spreadsheet file.
    std::ofstream fout;
    if (!options.filename.empty()) {
        fout.open(options.filename, std::ios_base::out | std::ios_base::binary);
    }

    // Capture error messages so we do not interfere with the CSV output stream.
    std::ostringstream err;

    const int result =
        stream_data_to_csv(fout.is_open() ? &fout : &std::cout, &err, options);
    if (result != 0) {
        std::cerr << err.str() << options.newline;
    }

    return result;
}

command_line_options::command_line_options()
    : message(), filename(), frames(), address("127.0.0.1"), port(32076),
      separator(","), newline("\n"), header(false)
{
}

int command_line_options::parse(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i) {
        const std::string arg(argv[i]);
        if ("--file" == arg) {
            ++i;
            if (i < argc) {
                filename = argv[i];
            } else {
                message = "Missing required argument for --file";
                return -1;
            }
        } else if ("--frames" == arg) {
            ++i;
            if (i < argc) {
                frames = std::stoi(argv[i]);
            } else {
                message = "Missing required argument for --frames";
                return -1;
            }
        } else if ("--header" == arg) {
            header = true;
        } else if ("--help" == arg) {
            return 1;
        } else {
            message = "Unrecognized option \"" + arg + "\"";
            return -1;
        }
    }

    return 0;
}

int command_line_options::print_help(std::ostream* out, char* program_name)
{
    if (!message.empty()) {
        *out << message << newline << newline;
    }

    *out << "Usage: " << program_name << " [options...]" << newline << newline
         << "Allowed options:" << newline
         << "  --help         show help message" << newline
         << "  --file arg     output file" << newline
         << "  --frames N     read N frames" << newline
         << "  --header       show channel names in the first row" << newline
         << newline;

    return 1;
}
