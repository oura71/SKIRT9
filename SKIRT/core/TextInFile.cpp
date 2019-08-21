/*//////////////////////////////////////////////////////////////////
////     The SKIRT project -- advanced radiative transfer       ////
////       © Astronomical Observatory, Ghent University         ////
///////////////////////////////////////////////////////////////// */

#include "TextInFile.hpp"
#include "FatalError.hpp"
#include "FilePaths.hpp"
#include "Log.hpp"
#include "System.hpp"
#include "Units.hpp"
#include <exception>
#include <regex>
#include <sstream>

////////////////////////////////////////////////////////////////////

namespace TextInFile_Private
{
    // private type to remember column info
    class ColumnInfo
    {
    public:
        ColumnInfo() { }
        string title;             // description specified in the file, used to remap columns
        string description;       // official description provided by the program
        string quantity;          // quantity, provided by the program
        string unit;              // unit, provided by the program or specified in the file
        double convFactor{1.};    // unit conversion factor from input to internal
        int waveExponent{0};      // wavelength exponent for converting "specific" quantities
        size_t waveIndex{0};      // index of wavelength column for converting "specific" quantities
    };
}

////////////////////////////////////////////////////////////////////

namespace
{
    // This function looks for the next header line that conforms to the required structured syntax. If such a line
    // is found, the column index, description and unit string are stored in the arguments and true is returned.
    // If no such header line is found, the function consumes the complete header and returns false.
    bool getNextInfoLine(std::ifstream& in, size_t& colIndex, string& description, string& unit)
    {
        // continue reading until a conforming header line is found or until the complete header has been consumed
        while (true)
        {
            // consume whitespace characters but nothing else
            while (true)
            {
                auto ch = in.peek();
                if (ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r') break;
                in.get();
            }

            // if the first non-whitespace character is not a hash character, there is no header line
            if (in.peek() != '#') return false;

            // read the header line
            string line;
            getline(in, line);

            // if the line conforms to the required syntax, return the extracted information
            static const std::regex syntax("#\\s*column\\s*(\\d+)\\s*:\\s*([^()]*)\\(\\s*([a-zA-Z0-9/]*)\\s*\\)\\s*",
                                           std::regex::icase);
            std::smatch matches;
            if (std::regex_match(line, matches, syntax) && matches.size()==4)
            {
                colIndex = std::stoul(matches[1].str());
                description = matches[2].str();
                unit = matches[3].str();
                return true;
            }
        }
    }

    // This function returns the wavelength exponent needed to convert a per wavelength / per frequency
    // quantity to internal (per wavelength) flavor, given the input units, or the error value of 99 if the
    // given units are not supported by any of the relevant quantities.
    int waveExponentForSpecificQuantity(Units* unitSystem, string unitString)
    {
        // a list of known per wavelength / per frequency quantities and the corresponding exponents
        static const vector<string> specificQuantities({
                           "wavelengthmonluminosity", "wavelengthfluxdensity", "wavelengthsurfacebrightness",
                           "neutralmonluminosity", "neutralfluxdensity", "neutralsurfacebrightness",
                           "frequencymonluminosity", "frequencyfluxdensity", "frequencysurfacebrightness"});
        static const vector<int> specificExponents({0,0,0, -1,-1,-1, -2,-2,-2});

        // loop over the list
        for (size_t q=0; q!=specificQuantities.size(); ++q)
        {
            // if this quantity supports the given unit, return the corresponding exponent
            if (unitSystem->has(specificQuantities[q], unitString)) return specificExponents[q];
        }
        return 99;  // error value
    }

    // This function returns the index of the first column in the given list that is described as "wavelength",
    // or the error value of 99 if there is no such column.
    size_t waveIndexForSpecificQuantity(const vector<TextInFile_Private::ColumnInfo*>& colv)
    {
        size_t index = 0;
        for (auto col : colv)
        {
            if (col->description == "wavelength") return index;
            index++;
        }
        return 99;  // error value
    }
}

////////////////////////////////////////////////////////////////////

TextInFile::TextInFile(const SimulationItem* item, string filename, string description)
{
    // open the file
    string filepath = item->find<FilePaths>()->input(filename);
    _in = System::ifstream(filepath);
    if (!_in) throw FATALERROR("Could not open the " + description + " text file " + filepath);

    // remember the units system and the logger
    _units = item->find<Units>();
    _log = item->find<Log>();

    // log "reading file" message
    _log->info( item->typeAndName() + " reads " + description + " from text file " + filepath + "...");

    // read any structured header lines into a list of ColumnInfo records
    size_t index;
    string title;
    string unit;
    while (getNextInfoLine(_in, index, title, unit))
    {
        // add a default-constructed ColumnInfo record to the list
        _colv.emplace_back(new TextInFile_Private::ColumnInfo);
        if (index != _colv.size())
            throw FATALERROR("Incorrect column index in file header for column " + std::to_string(_colv.size()));

        // remember the description and the units specified in the file
        _colv.back()->unit = unit;
        _colv.back()->title = title;
    }
    _numFileCols = _colv.size();
}

////////////////////////////////////////////////////////////////////

void TextInFile::close()
{
    if (_in.is_open())
    {
        _in.close();

        // log "done" message, except if an exception has been thrown
        if (!std::uncaught_exception()) _log->info("Done reading");
    }
}

////////////////////////////////////////////////////////////////////

TextInFile::~TextInFile()
{
    close();

    // delete column info structures
    for (auto col : _colv) delete col;
}

////////////////////////////////////////////////////////////////////

void TextInFile::addColumn(string description, string quantity, string defaultUnit)
{
    // if the file has no header info at all, add a default record for this column
    if (!_numFileCols)
    {
        _colv.emplace_back(new TextInFile_Private::ColumnInfo);
        _colv.back()->unit = defaultUnit;
    }
    // otherwise verify that there is a column specification to match this program column index
    else
    {
        if (_programColIndex+1 > _numFileCols)
            throw FATALERROR("No column info in file header for column " + std::to_string(_programColIndex+1));
    }

    // get a pointer to the column record being handled, and increment the program column index
    auto col = _colv[_programColIndex++];

    // store the programmatically provided information in the record (unit is already stored)
    col->description = description;
    col->quantity = quantity;

    // verify units and determine conversion factor for this column
    if (col->quantity.empty())       // dimensionless quantity
    {
        if (!col->unit.empty() && col->unit != "1")
            throw FATALERROR("Invalid units for dimensionless quantity in column " + std::to_string(_programColIndex));
        col->unit = "1";
    }
    else if (col->quantity == "specific")    // arbitrarily scaled value per wavelength or per frequency
    {
        col->waveExponent = waveExponentForSpecificQuantity(_units, col->unit);
        if (col->waveExponent == 99)
            throw FATALERROR("Invalid units for specific quantity in column " + std::to_string(_programColIndex));
        if (col->waveExponent)
        {
            col->waveIndex = waveIndexForSpecificQuantity(_colv);
            if (col->waveIndex == 99) throw FATALERROR("No preceding wavelength column for specific quantity in column "
                                                       + std::to_string(_programColIndex));
        }
    }
    else
    {
        if (!_units->has(col->quantity, col->unit))
            throw FATALERROR("Invalid units for quantity in column " + std::to_string(_programColIndex));
        col->convFactor = _units->in(col->quantity, col->unit, 1.);
    }

    // log column information
    string message = "  Column " + std::to_string(_programColIndex) + ": " + col->description + " (" + col->unit + ")";
    if (!col->title.empty()) message += " <-- " + col->title;
    _log->info(message);
}

////////////////////////////////////////////////////////////////////

bool TextInFile::readRow(Array& values)
{
    size_t ncols = _colv.size();
    if (!ncols) throw FATALERROR("No columns were declared for column text file");

    // read new line until it is non-empty and non-comment
    string line;
    while (_in.good())
    {
        getline(_in,line);
        auto pos = line.find_first_not_of(" \t");
        if (pos!=string::npos && line[pos]!='#')
        {
            // resize result array if needed (we don't need it to be cleared)
            if (values.size() != ncols) values.resize(ncols);

            // convert values from line and store them in result array
            std::stringstream linestream(line);
            for (size_t i=0; i<ncols; ++i)
            {
                auto col = _colv[i];

                // convert the value to floating point
                if (linestream.eof()) throw FATALERROR("One or more required value(s) on text line are missing");
                double value;
                linestream >> value;
                if (linestream.fail()) throw FATALERROR("Input text is not formatted as a floating point number");

                // convert from input units to internal units
                values[i] = value * (col->waveExponent ? pow(values[col->waveIndex],col->waveExponent)
                                                       : col->convFactor);
            }
            return true;
        }
    }

    // end of file was reached
    return false;
}

////////////////////////////////////////////////////////////////////

bool TextInFile::readNonLeaf(int& nx, int& ny, int& nz)
{
    string line;

    while (true)
    {
        int c = _in.peek();

        // skip comments line
        if (c=='#')
        {
            getline(_in,line);
        }

        // process nonleaf line
        else if (c=='!')
        {
            _in.get();              // skip exclamation mark
            getline(_in,line);

            // convert nx,ny,nz values from line and store them in output arguments
            std::stringstream linestream(line);
            linestream >> nx >> ny >> nz;
            if (linestream.fail())
                throw FATALERROR("Nonleaf subdivision specifiers are missing or not formatted as integers");

            return true;
        }

        // eat leading white space and empty lines
        else if (c==' ' || c=='\t' || c=='\n' || c=='\r')
        {
            _in.get();
        }

        // signal not a nonleaf line
        else
        {
            return false;
        }
    }
}

////////////////////////////////////////////////////////////////////

vector<Array> TextInFile::readAllRows()
{
    vector<Array> rows;
    while (true)
    {
        rows.emplace_back();        // add a default-constructed array to the vector
        if (!readRow(rows.back()))  // read next line's values into that array
        {
            rows.pop_back();        // at the end, remove the extraneous array
            break;
        }
    }
    return rows;
}

////////////////////////////////////////////////////////////////////

vector<Array> TextInFile::readAllColumns()
{
    // read the remainder of the file into rows
    const vector<Array>& rows = readAllRows();
    size_t nrows = rows.size();
    size_t ncols = _colv.size();

    // transpose the result into columns
    vector<Array> columns(ncols, Array(nrows));
    for (size_t c=0; c!=ncols; ++c)
        for (size_t r=0; r!=nrows; ++r)
            columns[c][r] = rows[r][c];

    return columns;
}


////////////////////////////////////////////////////////////////////
