/*//////////////////////////////////////////////////////////////////
////     The SKIRT project -- advanced radiative transfer       ////
////       © Astronomical Observatory, Ghent University         ////
///////////////////////////////////////////////////////////////// */

#include "SimulationItem.hpp"
#include "FatalError.hpp"

////////////////////////////////////////////////////////////////////

void SimulationItem::setup()
{
    if (_setupStarted) return;
    _setupStarted = true;

    setupSelfBefore();
    for (Item* child : children())
    {
        SimulationItem* item = dynamic_cast<SimulationItem*>(child);
        if (item) item->setup();
    }
    setupSelfAfter();
}

////////////////////////////////////////////////////////////////////

void SimulationItem::setupSelfBefore()
{
}

////////////////////////////////////////////////////////////////////

void SimulationItem::setupSelfAfter()
{
}

////////////////////////////////////////////////////////////////////

string SimulationItem::typeAndName() const
{
    string result = type();
    string name = itemName();
    if (!name.empty()) result += " " + name;
    return result;
}

////////////////////////////////////////////////////////////////////

std::string SimulationItem::itemName() const
{
    return string();
}

////////////////////////////////////////////////////////////////////

Item* SimulationItem::find(bool setup, SimulationItem* castToRequestedType(Item*)) const
{
    // loop over all ancestors
    Item* ancestor = const_cast<SimulationItem*>(this);  // cast away const
    while (ancestor)
    {
        // test the ancestor
        SimulationItem* candidate = castToRequestedType(ancestor);
        if (candidate)
        {
            if (setup) candidate->setup();
            return candidate;
        }

        // test its children
        for (Item* child : ancestor->children())
        {
            SimulationItem* candidate = castToRequestedType(child);
            if (candidate)
            {
                if (setup) candidate->setup();
                return candidate;
            }
        }

        // next ancestor
        ancestor = ancestor->parent();
    }

    if (setup) throw FATALERROR("No simulation item of requested type found in hierarchy");
    return nullptr;
}

////////////////////////////////////////////////////////////////////

SimulationItem* SimulationItem::interface(bool setup, bool implementsRequestedInterface(SimulationItem*)) const
{
    for (SimulationItem* candidate : interfaceCandidates())
    {
        if (implementsRequestedInterface(candidate))
        {
            if (setup) candidate->setup();
            return candidate;
        }
    }
    if (setup) throw FATALERROR("No simulation item implementing requested interface found in hierarchy");
    return nullptr;
}

////////////////////////////////////////////////////////////////////

vector<SimulationItem*> SimulationItem::interfaceCandidates() const
{
    vector<SimulationItem*> result({const_cast<SimulationItem*>(this)});  // cast away const
    if (parent())
        for (auto item : static_cast<SimulationItem*>(parent())->interfaceCandidates())
            result.push_back(item);
    return result;
}

////////////////////////////////////////////////////////////////////
