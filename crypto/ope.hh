#pragma once

#ifdef ONLINE_OPE
    std::cout << "Use online OPE ..." << std::endl;
    #include "ope-PRZB11.hh"
#else
    std::cout << "Use default OPE ..." << std::endl;
    #include "ope-KS88.hh"
#endif
