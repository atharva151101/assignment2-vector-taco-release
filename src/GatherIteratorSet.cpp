#include "GatherIteratorSet.h"

#include <set>

#include "IRVisitor.h"


void GatherIteratorSet::visit(const ArrayRead *ir) {
    const std::string &name = ir->access.name;
    if (seen.find(name) == seen.end()) {
        iterators.push_back(LIR::access_to_array_level(ir->access, formats));
        seen.insert(name);
    }
}

void GatherIteratorSet::visit(const ArrayAssignment *ir) {
    const std::string &name = ir->lhs.name;
    // assert(vars.empty());
    assert(seen.find(name) == seen.end()); // Should never see a written-to tensor twice.
    iterators.push_back(LIR::access_to_array_level(ir->lhs, formats));
    seen.insert(name);

    ir->rhs.accept(this);
}


LIR::IteratorSet gather_iterator_set(const IndexStmt &stmt, const FormatMap &formats) {
    GatherIteratorSet gatherer(formats);
    stmt.accept(&gatherer);
    return LIR::IteratorSet{gatherer.iterators};
}
