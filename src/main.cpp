// Copyright 2013-2016, Durachenko Aleksey V. <durachenko.aleksey@gmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
#include <QtArgumentParser>
#include <QCoreApplication>
#include <iostream>


void printUsage()
{
    std::cout
            << "mirandadbrecovery Ver 0.1.0 [2016.01.27"        << std::endl
            << "    Recovery the miranda database"              << std::endl
            << "Usage:"                                         << std::endl
            << "    mirandadbrecovery -i miranda.db -o output.ext "
            << "-f <output_format> [--verbose]"                 << std::endl
            << "Options:"                                       << std::endl
            << "    -i input file name"                         << std::endl
            << "    -o output file name"                        << std::endl
            << "    -f output format ['sqlite']"                << std::endl;
}


int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QtArgumentParser parser(QCoreApplication::arguments());
    parser.add("-i", QtArgumentParser::String);
    parser.add("-o", QtArgumentParser::String);
    parser.add("-f", QtArgumentParser::Variant, QStringList() << "sqlite");
    parser.add("--verbose", QtArgumentParser::Flag);

    if (!parser.parse()) {
        std::cout << "cannot parse the arguments: "
                  << parser.errorString().toStdString()
                  << std::endl;
        return -1;
    }

    QVariantMap map = parser.result();
    if (!map.contains("-i") || !map.contains("-o") || !map.contains("-f")) {
        printUsage();
        return -1;        
    }

    const QString input = map.value("-i").toString();
    const QString output = map.value("-o").toString();
    const QString format = map.value("-f").toString();
    const bool verbose = map.value("--verbose").toBool();

    std::cout << "== Summary ==" << std::endl;
    std::cout << "  MirandaDb filename: " << input.toStdString() << std::endl;
    std::cout << "  Output filename   : " << output.toStdString() << std::endl;
    std::cout << "  Output format     : " << format.toStdString() << std::endl;
    std::cout << "  Verbose           : " << (verbose ? "true" : "false") << std::endl;



    return 0;
}
