#include <iostream>

#include "generator/HRR_Algorithm_Base.hpp"

using namespace std;

void HRR_Algorithm_Base::HRRDoubletLoop(HRRDoubletStepList & hrrlist,
                                        const DoubletSet & inittargets,
                                        DoubletSet & solveddoublets)
{
    DoubletSet targets = inittargets;

    while(targets.size())
    {
        DoubletSet newtargets;

        for(auto it = targets.rbegin(); it != targets.rend(); ++it)
        {
            // skip if done alread
            // this can happen with some of the more complex trees
            if(solveddoublets.count(it) > 0)
                continue;

            HRRDoubletStep hrr = this->doubletstep(*it);
            hrrlist.push_back(hrr);

            if(solveddoublets.count(hrr.src1) == 0)
                newtargets.insert(hrr.src1);
            if(solveddoublets.count(hrr.src2) == 0)
                newtargets.insert(hrr.src2);
            
            solveddoublets.insert(it);
        }

        cout << "Generated " << newtargets.size() << " new targets\n";

        PruneRight(newtargets);
        cout << "After pruning: " << newtargets.size() << " new targets\n";
        for(const auto & it : newtargets)
            cout << "    " << it << "\n";

        targets = newtargets;
    } 

}



HRRBraKetStepList HRR_Algorithm_Base::Create_DoubletStepLists(QAM amlist)
{
    // First, we need a list of doublet steps for the bra
    HRRDoubletStepList bralist;

    // holds all the 'solved' doublets
    DoubletSet solvedbras;

    // generate initial targets
    DoubletSet initbras = GenerateInitialDoubletTargets({amlist[0],amlist[1]}, DoubletType::BRA, true);
    PrintDoubletSet(initbras, "Initial Targets");

    // Inital bra targets
    DoubletSet targets = initbras;
    PruneRight(targets);
    PrintDoubletSet(targets, "Inital bra targets");

    // Solve the bra part
    HRRDoubletLoop(bralist, targets, solvedbras);
    std::reverse(bralist.begin(), bralist.end());

    cout << "\n\n";
    cout << "--------------------------------------------------------------------------------\n";
    cout << "BRA HRR step done. Solution is " << bralist.size() << " steps\n";
    cout << "--------------------------------------------------------------------------------\n";
    for(auto & it : bralist)
        cout << it << "\n";
    

    // now do kets
    // we only need the bras from the original targets
    HRRDoubletStepList ketlist;
    DoubletSet solvedkets;
    DoubletSet initkets = GenerateInitialDoubletTargets({amlist[2],amlist[3]}, DoubletType::KET, true);
    targets = initkets;
    PruneRight(targets);

    cout << "\n\n";
    PrintDoubletSet(targets, "Initial ket targets");

    // Solve the ket part
    HRRDoubletLoop(ketlist, targets, solvedkets);
    std::reverse(ketlist.begin(), ketlist.end());

    cout << "\n\n";
    cout << "--------------------------------------------------------------------------------\n";
    cout << "KET HRR step done. Solution is " << ketlist.size() << " steps\n";
    cout << "--------------------------------------------------------------------------------\n";
    for(auto & it : ketlist)
        cout << it << "\n";

    cout << "\n\n";

    // mark top level reqs
    for(const auto & it : bralist)
    {
        if(solvedbras.count(it.src1) == 0)
            bratop_.insert(it.src1);
        if(solvedbras.count(it.src2) == 0)
            bratop_.insert(it.src2);
    } 
    for(const auto & it : ketlist)
    {
        if(solvedkets.count(it.src1) == 0)
            kettop_.insert(it.src1);
        if(solvedkets.count(it.src2) == 0)
            kettop_.insert(it.src2);
    }

    for(const auto & it : bratop_)
        bratopam_.insert(it.amlist());
    for(const auto & it : kettop_)
        kettopam_.insert(it.amlist());

    // form top quartets
    // all top bra/ket combo
    for(const auto & it : bratop_)
    for(const auto & it2 : kettop_)
        topquartets_.insert({it, it2, 0});

    for(const auto & it : topquartets)
        topqam_.insert(it.amlist());

    return HRRBraKetStepList(bralist, ketlist);
}

std::pair<DAMset, DAMSet> HRR_Algorithm_Base::TopBraKetAM(void) const
{
    return {bratopam_, kettopam_};
}

QAMSet HRR_Algorithm_Base::TopQAM(void) const
{
    return topqam_;
}

QuartetSet HRR_Algorithm_Base::TopQuartets(void) const
{
    return topquartets_;
}
