
#include "Lattice.h"

#include <algorithm>
#include <iostream>
#include <ostream>
#include <set>

#include "Expr.h"
#include "IRVisitor.h"
#include "IRPrinter.h"
#include "SetExpr.h"
#include "GatherIteratorSet.h"
#include "SetExprUtils.h"


namespace {

// For debugging.
void print_merge_lattice(std::ostream &stream, const MergeLattice &lattice) {
    for (const auto &p : lattice.node_map) {
        stream << "--------------------\n";
        for (const auto &i : p.second->iterators) {
            stream << i.name << "_i_" << ((i.format == Format::Compressed) ? "c" : "d") << "\n";
        }
        stream << "----------\n";
        stream << p.second->body;
        stream << "--------------------\n";
    }
}

}  // namespace


MergePoint* build_merge_lattice(const SetExpr &sexpr, const IndexStmt &body, const FormatMap &formats, MergeLattice &lattice) {
    if(lattice.node_map.find(sexpr) != lattice.node_map.end()) {
        // Node already exists
        return lattice.node_map[sexpr];
    }

    auto [iterators, locators] = split_iterators_locators(sexpr, formats);
    // if no iterators are left (everything is dense, use any dense locator as an iterator)
    if (iterators.empty()) {
        assert(!locators.empty());
        iterators.push_back(locators.front());
    }
    auto *new_point = new MergePoint{.sexpr = sexpr, .iterators = std::move(iterators), .locators = std::move(locators), .children = {}, .body = body};

    std::vector<MergePoint*> children;
    for(auto iterator : new_point->iterators) {
        auto newSetExpr = get_simplified_set_expr(sexpr, iterator);
        if(!newSetExpr.defined()) continue;
        auto newBody = get_simplified_index_stmt(body, newSetExpr, formats);
        children.push_back(build_merge_lattice(newSetExpr, newBody, formats, lattice));
    }
    new_point->children = std::move(children);
    lattice.node_map[sexpr] = new_point;
    return new_point;
}

MergeLattice MergeLattice::make(const SetExpr &sexpr, const IndexStmt &body, const FormatMap &formats) {
    MergeLattice lattice;
    lattice.root = build_merge_lattice(sexpr, body, formats, lattice);
    return lattice;
}


void toposortDfs(const MergePoint &point, std::map<SetExpr, bool, SetComparator> &visited, std::vector<const MergePoint*> &result) {
    if(visited.find(point.sexpr) != visited.end()) {
        return;
    }
    visited[point.sexpr] = true;
    for(auto child : point.children) {
        toposortDfs(*child, visited, result);
    }
    result.push_back(&point);
}

std::vector<const MergePoint*> MergeLattice::get_sub_points(const MergePoint &point) const {
    std::vector<const MergePoint*> out;
    std::map<SetExpr, bool, SetComparator> visited;

    toposortDfs(point, visited, out);
    std::reverse(out.begin(), out.end());

    if (!out.empty() && out.front() == &point) {
        out.erase(out.begin());
    }
    return out;
}

