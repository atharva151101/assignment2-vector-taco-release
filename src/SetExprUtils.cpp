#include "SetExpr.h"
#include "IRVisitor.h"
#include "SetExprUtils.h"
#include "Format.h"

struct IRVisitor;


bool SetComparator::operator()(const SetExpr &lhs, const SetExpr &rhs) const {
    if (!lhs.defined() && !rhs.defined()) return false;
    if (!lhs.defined()) return true;
    if (!rhs.defined()) return false;

    if (lhs.ptr->kind != rhs.ptr->kind)
        return lhs.ptr->kind < rhs.ptr->kind;

    auto binop = [&](auto const& l, auto const& r) -> bool {
        if ((*this)(l->a, r->a)) return true;
        if ((*this)(r->a, l->a)) return false;
        if ((*this)(l->b, r->b)) return true;
        if ((*this)(r->b, l->b)) return false;
        return false;
    };

    if (lhs.ptr->kind == SetExprKind::ArrayDim) {
        auto l = std::dynamic_pointer_cast<const ArrayDim>(lhs.ptr);
        auto r = std::dynamic_pointer_cast<const ArrayDim>(rhs.ptr);
        assert(l && r);
        return l->access.name < r->access.name;
    } else if (lhs.ptr->kind == SetExprKind::Union) {
        auto l = std::dynamic_pointer_cast<const Union>(lhs.ptr);
        auto r = std::dynamic_pointer_cast<const Union>(rhs.ptr);
        assert(l && r);
        return binop(l, r);
    } else if (lhs.ptr->kind == SetExprKind::Intersection) {
        auto l = std::dynamic_pointer_cast<const Intersection>(lhs.ptr);
        auto r = std::dynamic_pointer_cast<const Intersection>(rhs.ptr);
        assert(l && r);
        return binop(l, r);
    }

    assert(false && "Unreachable");
    return false;
}


struct Simplify : public IRVisitor {
        SetExpr sexpr;
        bool is_empty = false;
        const LIR::ArrayLevel &remove_iterator;

        Simplify(const SetExpr &sexpr, const LIR::ArrayLevel &remove_iterator)
            : sexpr(sexpr), remove_iterator(remove_iterator) {}


        virtual void visit(const ArrayDim *arrayDim) override {
            if(arrayDim->access.name == remove_iterator.name) {
                sexpr = SetExpr();
                if(remove_iterator.format == Format::Compressed) {
                    is_empty = true;
                } else {
                    is_empty = false;
                }
                return;
            }

            sexpr = ArrayDim::make(arrayDim->access);
        }

        virtual void visit(const Union *unionNode) override {
            unionNode->a.accept(this);
            auto a = std::move(sexpr);
            if(!a.defined() && !is_empty) {
                sexpr = SetExpr();
                is_empty = false;
                return;
            }
            unionNode->b.accept(this);
            auto b = std::move(sexpr);
            if(!b.defined() && !is_empty) {
                sexpr = SetExpr();
                is_empty = false;
                return;
            }

            if(a.defined() && b.defined()) {
                sexpr = Union::make(std::move(a), std::move(b));
            } else if(a.defined()) {
                sexpr = std::move(a);
            } else if(b.defined()) {
                sexpr = std::move(b);
            } else {
                sexpr = SetExpr();
                is_empty = true;
            }

        }

        virtual void visit(const Intersection *intersectionNode) override {
            intersectionNode->a.accept(this);
            auto a = std::move(sexpr);
            if(!a.defined()) {
                sexpr = SetExpr();
                is_empty = true;
                return;
            }
            intersectionNode->b.accept(this);
            auto b = std::move(sexpr);
            if(!b.defined()) {
                sexpr = SetExpr();
                is_empty = true;
                return;
            }
            
            sexpr = Intersection::make(std::move(a), std::move(b));
    
        }
    };

SetExpr get_simplified_set_expr(const SetExpr &sexpr, const LIR::ArrayLevel &remove_iterator) {
    Simplify simplifier(sexpr, remove_iterator);
    sexpr.accept(&simplifier);
    return std::move(simplifier.sexpr);
}

std::set<SetExpr, SetComparator> get_iterators(const SetExpr &sexpr, const FormatMap &formats) {
    struct CollectIterators : public IRVisitor {
        std::set<SetExpr, SetComparator> iterators;
        const FormatMap &formats;

        CollectIterators(const FormatMap &formats) : formats(formats) {}

        virtual void visit(const ArrayDim *arrayDim) override {
            iterators.insert(ArrayDim::make(arrayDim->access));
        }
    };

    CollectIterators collector(formats);
    sexpr.accept(&collector);
    return collector.iterators;
}


std::pair<std::vector<LIR::ArrayLevel>, std::vector<LIR::ArrayLevel>> split_iterators_locators(SetExpr sexpr, const FormatMap &formats) {
    std::vector<LIR::ArrayLevel> iterators;
    std::vector<LIR::ArrayLevel> locators;

    struct GetSparseMap : public IRVisitor {
        std::map<SetExpr, bool, SetComparator> sparse_map;
        const FormatMap &formats;

        GetSparseMap(const FormatMap &formats) : formats(formats) {}

        virtual void visit(const ArrayDim *arrayDim) override {
            std::string name = arrayDim->access.name;
           sparse_map[ArrayDim::make(arrayDim->access)] = (formats.at(name)[0] == Format::Compressed);
        }

        virtual void visit(const Intersection *intersectionNode) override {
            intersectionNode->a.accept(this);
            intersectionNode->b.accept(this);
            sparse_map[Intersection::make(std::move(intersectionNode->a.ptr), std::move(intersectionNode->b.ptr))] = sparse_map[SetExpr(intersectionNode->a.ptr)] && sparse_map[SetExpr(intersectionNode->b.ptr)];
        }

        virtual void visit(const Union *unionNode) override {
            unionNode->a.accept(this);
            unionNode->b.accept(this);
            sparse_map[Union::make(std::move(unionNode->a.ptr), std::move(unionNode->b.ptr))] = sparse_map[SetExpr(unionNode->a.ptr)] || sparse_map[SetExpr(unionNode->b.ptr)];
        }

    };
    GetSparseMap sparseMapGetter(formats);
    sexpr.accept(&sparseMapGetter);
    auto sparseMap = sparseMapGetter.sparse_map;

   struct IteratorLocator : public IRVisitor {
       std::vector<LIR::ArrayLevel> iterators;
       std::vector<LIR::ArrayLevel> locators;
       std::map<SetExpr, bool, SetComparator>& sparse_map;
       const FormatMap &formats;
       bool is_under_intersect = false;
       bool is_under_dense_union = false;
       IteratorLocator(std::map<SetExpr, bool, SetComparator>& sparse_map, const FormatMap &formats)
           : sparse_map(sparse_map), formats(formats) {}
       virtual void visit(const ArrayDim *arrayDim) override {
           auto node = ArrayDim::make(arrayDim->access);
           if(is_under_intersect && !sparse_map[node]) {
               locators.push_back(LIR::access_to_array_level(arrayDim->access, formats));
           } else if(is_under_dense_union && !sparse_map[node]) {
               locators.push_back(LIR::access_to_array_level(arrayDim->access, formats));
           } else {
               iterators.push_back(LIR::access_to_array_level(arrayDim->access, formats));   
           }
       }

        virtual void visit(const Intersection *intersectionNode) override {
                bool prev_intersect = is_under_intersect;
                is_under_intersect = true;
                intersectionNode->a.accept(this);
                intersectionNode->b.accept(this);
                is_under_intersect = prev_intersect;
        }

        virtual void visit(const Union *unionNode) override {
            if(!sparse_map[Union::make(std::move(unionNode->a.ptr), std::move(unionNode->b.ptr))]) {
                bool prev_dense_union = is_under_dense_union;
                is_under_dense_union = true;
                unionNode->a.accept(this);
                unionNode->b.accept(this);
                is_under_dense_union = prev_dense_union;
            } else {
                unionNode->a.accept(this);
                unionNode->b.accept(this);
            }
        }

   };

   IteratorLocator locator(sparseMap, formats);
   sexpr.accept(&locator);

   return {locator.iterators, locator.locators};
}


IndexStmt get_simplified_index_stmt(const IndexStmt &stmt, const SetExpr &sexpr, const FormatMap &formats) {
    auto assign_stmt = std::dynamic_pointer_cast<const ArrayAssignment>(stmt.ptr);
    assert(assign_stmt != nullptr);

    struct SimplifyBody : public IRVisitor {
        Expr simplified_expr;
        const std::set<SetExpr, SetComparator> &iterators;

        SimplifyBody(const std::set<SetExpr, SetComparator> &iterators)
            : iterators(iterators) {}

        virtual void visit(const ArrayRead * arrayread) {
            if(iterators.find(ArrayDim::make(arrayread->access)) != iterators.end()) {
                simplified_expr = ArrayRead::make(arrayread->access);
            } else {
                simplified_expr = Expr();
            }
        }
        virtual void visit(const Add * add) {
            add->a.accept(this);
            auto lhs = std::move(simplified_expr);
            add->b.accept(this);
            auto rhs = std::move(simplified_expr);
            if(lhs.defined() && rhs.defined()) {
                simplified_expr = Add::make(lhs, rhs);
            } else if(lhs.defined()) {
                simplified_expr = std::move(lhs);
            } else if(rhs.defined()) {
                simplified_expr = std::move(rhs);
            } else {
                simplified_expr = Expr();
            }
        }
        virtual void visit(const Mul * mul) {
            mul->a.accept(this);
            auto lhs = std::move(simplified_expr);
            mul->b.accept(this);
            auto rhs = std::move(simplified_expr);
            if(lhs.defined() && rhs.defined()) {
                simplified_expr = Mul::make(lhs, rhs);
            } else {
                simplified_expr = Expr();
            }
        }
    };
    auto iters = get_iterators(sexpr, formats);

    SimplifyBody simplifier(iters);
    assign_stmt->rhs.accept(&simplifier);

    return ArrayAssignment::make(assign_stmt->lhs, simplifier.simplified_expr);
}