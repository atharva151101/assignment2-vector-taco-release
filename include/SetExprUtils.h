#pragma once
#include "SetExpr.h"
#include <set>
#include "LIR.h"
#include "IndexStmt.h"

// Comparator for comparing to set expressions, used for storing set expressions in a map or set
// This helps to deduplicate merge points for same set expression.
struct SetComparator{
    bool operator()(const SetExpr &lhs, const SetExpr &rhs) const;
};

SetExpr get_simplified_set_expr(const SetExpr &sexpr, const LIR::ArrayLevel &remove_iterator);
std::pair<std::vector<LIR::ArrayLevel>, std::vector<LIR::ArrayLevel>> split_iterators_locators(SetExpr sexpr, const FormatMap &formats);

IndexStmt get_simplified_index_stmt(const IndexStmt &stmt, const SetExpr &sexpr, const FormatMap &formats);

