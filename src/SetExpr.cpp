#include "SetExpr.h"
#include "IRVisitor.h"

void SetExpr::accept(IRVisitor *v) const {
    ptr->accept(v);
}

SetExpr operator|(SetExpr a, SetExpr b) {
    return Union::make(a, b);
}

SetExpr operator&(SetExpr a, SetExpr b) {
    return Intersection::make(a, b);
}

void ArrayDim::accept(IRVisitor *v) const {
    v->visit(this);
}

const std::shared_ptr<const ArrayDim> ArrayDim::make(const Access &_access) {
    auto node = std::make_shared<ArrayDim>(_access);
    node->kind = SetExprKind::ArrayDim;
    return node;
}

const std::shared_ptr<const Union> Union::make(SetExpr _a, SetExpr _b) {
    auto node = std::make_shared<Union>(_a, _b);
    node->kind = SetExprKind::Union;
    return node;
}

void Union::accept(IRVisitor *v) const {
    v->visit(this);
}

const std::shared_ptr<const Intersection> Intersection::make(SetExpr _a, SetExpr _b) {
    auto node = std::make_shared<Intersection>(_a, _b);
    node->kind = SetExprKind::Intersection;
    return node;
}

void Intersection::accept(IRVisitor *v) const {
    v->visit(this);
}
