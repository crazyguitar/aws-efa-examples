#include "common/handle.h"

#include "common/io.h"

void Handle::schedule() {
  if (state_ == Handle::kUnschedule) {
    IO::Get().Call(*this);
  }
}

void Handle::cancel() {
  if (state_ != Handle::kUnschedule) {
    IO::Get().Cancel(*this);
  }
}
