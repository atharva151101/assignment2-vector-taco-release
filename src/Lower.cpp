#include "Lower.h"

#include <algorithm>
#include <set>

#include "IRVisitor.h"
#include "IRPrinter.h"
#include "GatherIteratorSet.h"
#include "Lattice.h"


IndexStmt lower(const Assignment &assignment) {
    IndexStmt cin = ArrayAssignment::make(
        assignment.access,
        assignment.rhs
    );

    struct SetExprLowerer : public IRVisitor {
        SetExpr setExpr;
        virtual void visit(const ArrayRead * arrayread) {
            setExpr = ArrayDim::make(arrayread->access);
        }
        virtual void visit(const Add * add) {
            add->a.accept(this);
            auto lhs = std::move(setExpr);
            add->b.accept(this);
            auto rhs = std::move(setExpr);
            setExpr = Union::make(lhs, rhs);
        }
        virtual void visit(const Mul * mul) {
            mul->a.accept(this);
            auto lhs = std::move(setExpr);
            mul->b.accept(this);
            auto rhs = std::move(setExpr);
            setExpr = Intersection::make(lhs, rhs);
        }
    };
    SetExprLowerer lowerer;
    assignment.rhs.accept(&lowerer);
    cin = ForAll::make(lowerer.setExpr, cin);
    return cin;
}

LIR::Stmt lower(const IndexStmt &stmt, const FormatMap &formats) {
    auto forall = std::dynamic_pointer_cast<const ForAll>(stmt.ptr);
    MergeLattice lattice = MergeLattice::make(forall->sexpr, forall->body, formats);

    auto lower_assign_stmt = [&](const MergePoint &point) {
        IndexStmt stmt = point.body;
        auto assign_stmt = std::dynamic_pointer_cast<const ArrayAssignment>(stmt.ptr);
        assert(assign_stmt != nullptr);

        struct LExprLowerer : public IRVisitor {
            LIR::Expr lir_expr;
            FormatMap formats;
            LExprLowerer(const FormatMap &formats) : formats(formats) {}

            virtual void visit(const ArrayRead *arrayread) {
                lir_expr = LIR::ArrayAccess::make(LIR::access_to_array_level(arrayread->access, formats));
            }

            virtual void visit(const Add *add) {
                add->a.accept(this);
                auto a = std::move(lir_expr);
                add->b.accept(this);
                auto b = std::move(lir_expr);
                lir_expr = LIR::Add::make(a, b);
            }

            virtual void visit(const Mul *mul) {
                mul->a.accept(this);
                auto a = std::move(lir_expr);
                mul->b.accept(this);
                auto b = std::move(lir_expr);
                lir_expr = LIR::Mul::make(a, b);
            }
        };
        LExprLowerer lowerer(formats);
        assign_stmt->rhs.accept(&lowerer);

        return LIR::ArrayAssignment::make(LIR::access_to_array_level(assign_stmt->lhs, formats), lowerer.lir_expr);
    };

    auto lower_while_loop = [&](const MergePoint &point) {
        std::vector<LIR::Stmt> body;
        auto iters = point.iterators;
        for (const auto &iter : iters) {
            if(iter.format == Format::Compressed) {
                body.push_back(LIR::CompressedIndexDefinition::make(iter));
            }
        }

        body.push_back(LIR::LogicalIndexDefinition::make(LIR::IteratorSet{iters}));

        auto sub_points = lattice.get_sub_points(point);

        std::vector<LIR::IteratorSet> if_conditions;
        std::vector<LIR::Stmt> if_bodies;
        if_conditions.push_back(LIR::IteratorSet{point.iterators});
        if_bodies.push_back(lower_assign_stmt(point));
        for (const auto &sub_point : sub_points) {
            if_conditions.push_back(LIR::IteratorSet{sub_point->iterators});
            if_bodies.push_back(lower_assign_stmt(*sub_point));
        }
        if(if_conditions.size() > 1 || point.iterators.size() > 1) {
            body.push_back(LIR::IfStmt::make(if_conditions, if_bodies));
        } else {
            body.push_back(if_bodies[0]);
        }

        for (const auto &iter : iters) {
            body.push_back(LIR::IncrementIterator::make(iter, !(iter.format == Format::Compressed) || iters.size() == 1));
        }

        return LIR::WhileStmt::make(
            LIR::IteratorSet{iters},
            LIR::SequenceStmt::make(body)
        );
    };

    std::vector<LIR::Stmt> stmts;

    stmts.push_back(LIR::IteratorDefinition::make(LIR::IteratorSet{lattice.root->iterators}));

    stmts.push_back(lower_while_loop(*lattice.root));
    for (const auto &sub_point : lattice.get_sub_points(*lattice.root)) {
        stmts.push_back(lower_while_loop(*sub_point));
    }

    return LIR::SequenceStmt::make(stmts);
}
