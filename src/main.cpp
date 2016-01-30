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
#include "miranda.h"
#include <QtArgumentParser>
#include <QCoreApplication>
#include <iostream>


void printUsage()
{
    std::cout << "mirandadbrecovery v.1.0"              << std::endl
              << "    Recovery the miranda database"    << std::endl
              << "Usage:"                               << std::endl
              << "    mirandadbrecovery -i miranda.db -o output.json [-v]" << std::endl
              << "Options:"                             << std::endl
              << "    -i input miranda database"        << std::endl
              << "    -o output json file"              << std::endl;
}


int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QtArgumentParser parser(QCoreApplication::arguments());
    parser.add("-i", QtArgumentParser::String);
    parser.add("-o", QtArgumentParser::String);
    parser.add("-v", QtArgumentParser::Flag);

    if (!parser.parse()) {
        std::cout << "cannot parse the arguments: "
                  << parser.errorString().toStdString()
                  << std::endl;
        return -1;
    }

    QVariantMap map = parser.result();
    if (!map.contains("-i") || !map.contains("-o")) {
        printUsage();
        return -1;
    }

    const QString input = map.value("-i").toString();
    const QString output = map.value("-o").toString();
    const bool verbose = map.value("-v").toBool();

    std::cout << "== Summary ==" << std::endl;
    std::cout << "  Miranda database: " << input.toStdString() << std::endl;
    std::cout << "  Output json file: " << output.toStdString() << std::endl;
    std::cout << "  Verbose         : " << (verbose ? "true" : "false") << std::endl;

    if (miranda2json(input, output, verbose)) {
        return 0;
    }

    return 1;
}
