#include "execution-plan.h"

#include "all-transports.h"

//template ExecutionPlan<TCPSession>
//generate_execution_plan<TCPSession>(const DSLData &);

template<typename Session>
ExecutionPlan<Session> generate_execution_plan(const DSLData & script)
{
    // TODO: constexpr evaluation for each choice.

}
