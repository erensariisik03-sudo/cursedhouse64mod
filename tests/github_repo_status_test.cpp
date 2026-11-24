#include "github_repo_status.h"
#include <cassert>
using namespace github_repo_status;
int main() {
    static_assert(Classify(true,200)==State::Public);
    static_assert(Classify(true,404)==State::NotPublic);
    static_assert(Classify(true,403)==State::NetworkError);
    static_assert(Classify(true,429)==State::NetworkError);
    static_assert(Classify(false,0)==State::NetworkError);
    assert(Classify(true,500)==State::NetworkError);
}
