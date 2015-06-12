#include "generator/ET_Writer.hpp"
#include "generator/WriterBase.hpp"
#include "generator/ET_Algorithm_Base.hpp"
#include "generator/Helpers.hpp"


ET_Writer::ET_Writer(const ET_Algorithm_Base & et_algo) 
{ 
    etsl_ = et_algo.ETSteps();

    // see what we need for arrays
    for(const auto & it : etsl_)
    {
        if(it.src1)
            etint_.insert(it.src1.amlist());
        if(it.src2)
            etint_.insert(it.src2.amlist());
        if(it.src3)
            etint_.insert(it.src3.amlist());
        if(it.src3)
            etint_.insert(it.src4.amlist());
        if(it.target)
            etint_.insert(it.target.amlist());
    }
}



void ET_Writer::WriteIncludes(std::ostream & os, const WriterBase & base) const
{
}



void ET_Writer::DeclarePrimArrays(std::ostream & os, const WriterBase & base) const
{
    if(etint_.size())
    {
        os << indent6 << "// Holds temporary integrals for electron transfer\n";

        for(const auto & it : etint_)
        {
            // only if these aren't from vrr
            if(it[1] > 0 || it[2] > 0 || it[3] > 0)
                os << indent6 << base.DoubleType() << " " << base.PrimVarName(it) << "[" << NCART(it[0]) * NCART(it[2]) << "];\n";
        } 

        os << "\n\n";

    }
}


void ET_Writer::DeclarePrimPointers(std::ostream & os, const WriterBase & base) const
{
    if(etint_.size())
    {
        for(const auto & it : etint_)
        {
            if(base.IsContArray(it))
                os << indent4  << "double * const restrict " << base.PrimPtrName(it)
                   << " = " << base.ArrVarName(it) << " + abcd * " << NCART(it[0]) * NCART(it[2]) << ";\n";
        }

        os << "\n\n";

    }
}


void ET_Writer::WriteETInline(std::ostream & os, const WriterBase & base) const
{
    os << "\n";
    os << indent6 << "//////////////////////////////////////////////\n";
    os << indent6 << "// Primitive integrals: Electron transfer\n";
    os << indent6 << "//////////////////////////////////////////////\n";
    os << "\n";

    if(etsl_.size() == 0)
        os << indent6 << "//...nothing to do...\n";
    else
    {
        for(const auto & it : etsl_)
        {
            os << indent6 << "// " << it << "\n";
            os << ETStepString(it, base);
            os << "\n";
        }
    }

    // add to needed contracted integrals
    for(const auto & it : etint_)
    {
        if(base.IsContArray(it))
        {
            int ncart = NCART(it[0])*NCART(it[2]);

            os << "\n";
            os << indent6 << "// Accumulating in contracted workspace\n";

            os << indent6 << "for(n = 0; n < " << ncart << "; n++)\n";

            if(base.Intrinsics())
            {
                os << indent6 << "{\n";

                /*
                os << indent7 << "double vec[" << base.SimdLen() << "] __attribute__((aligned(" << base.ByteAlign() << ")));\n";
                os << indent7 << base.DoubleStore(base.PrimVarName(it) + "[n]", "vec", "") << ";\n";
                os << indent7 << base.PrimPtrName(it) << "[n] += vec[0]";
                */

                os << indent7 << "union double4 vec = (union double4)" << base.PrimVarName(it) << "[n];\n";    
                os << indent7 << base.PrimPtrName(it) << "[n] += vec.v[0]";

                for(int i = 1; i < base.SimdLen(); i++)
                    os << " + vec.v[" << i << "]";
                    //os << " + vec[" << i << "]";
                os << ";\n";
                os << indent6 << "}\n";
            }
            else
                os << indent7 << base.PrimPtrName(it) << "[n] += " << base.PrimVarName(it) << "[n];\n";
        }
    }
}



std::string ET_Writer::ETStepVar(const Quartet & q, const WriterBase & base)
{
    std::stringstream ss; 
    ss << base.PrimVarName(q.amlist()) << "[" << q.idx() << "]";
    return ss.str();
}



std::string ET_Writer::ETStepString(const ETStep & et, const WriterBase & base)
{
    int stepidx = XYZStepToIdx(et.xyz);
    std::stringstream ival;
    ival << et.target.bra.left.ijk[stepidx];
    std::stringstream kval;
    kval << (et.target.ket.left.ijk[stepidx]-1);

    std::stringstream ss;
    ss << indent6 <<  ETStepVar(et.target, base);

    ss << " = ";

    ss << "etfac[" << stepidx << "] * " << ETStepVar(et.src1, base);

    if(et.src2.bra.left && et.src2.ket.left)
        ss << " + " << base.DoubleSet(ival.str()) << " * one_over_2q * " << ETStepVar(et.src2, base);
    if(et.src3.bra.left && et.src3.ket.left)
        ss << " + " << base.DoubleSet(kval.str()) << " * one_over_2q * " << ETStepVar(et.src3, base);
    if(et.src4.bra.left && et.src4.ket.left)
        ss << " - p_over_q * " << ETStepVar(et.src4, base);
    ss << ";\n";

    return ss.str();
}

