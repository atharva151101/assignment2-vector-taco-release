#pragma once

#include <vector>

#include "IndexStmt.h"
#include "LIR.h"
#include "SetExpr.h"
#include "IRVisitor.h"
#include "SetExprUtils.h"
#include <map>
// One possible interface for implementing MergeLattices


struct MergePoint {
    SetExpr sexpr;
    std::vector<LIR::ArrayLevel> iterators;
    std::vector<LIR::ArrayLevel> locators;
    std::vector<MergePoint *> children;
    IndexStmt body;
};

struct MergeLattice {
    std::map<SetExpr, MergePoint*, SetComparator> node_map;
    MergePoint* root;

    static MergeLattice make(const SetExpr &sexpr, const IndexStmt &body, const FormatMap &formats);

    // Get all of the points from this MergeLattice that are sub-lattices of point,
    // all points that are dominated by point.
    std::vector<const MergePoint*> get_sub_points(const MergePoint &point) const;
};
