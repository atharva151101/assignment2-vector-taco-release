#pragma once

#include <vector>

#include "Format.h"
#include "IndexStmt.h"
#include "IRVisitor.h"
#include <set>
#include "LIR.h"

struct IRVisitor;

struct GatherIteratorSet : public IRVisitor {
    const FormatMap &formats;

    GatherIteratorSet(const FormatMap &_formats) : formats(_formats) {}

    std::vector<LIR::ArrayLevel> iterators;
    std::set<std::string> seen;

    void visit(const ArrayRead *ir) override ;

    void visit(const ArrayAssignment *ir) override ;
};


// Gathers iterators in in-order traversal.
LIR::IteratorSet gather_iterator_set(const IndexStmt &stmt, const FormatMap &formats);
