#include "Common.h"

ModuleChain* ModuleChain::head;

ModuleChain::ModuleChain(const char* name, byte aPrirority,  void (*h)(ModuleCmd)) : next(NULL), priority(aPrirority) {
  static ModuleChain* __last = NULL;
  if (__last == NULL) {
    head = NULL;
  }
  __last = this;
  handler = h;
  if (head == NULL) {
    head = this;
    return;
  }
  if (head->priority >= aPrirority) {
    next = head;
    head = this;
    return;
  }
  ModuleChain* p = head;
  while (p->priority < aPrirority) {
    if (p->next == NULL) {
      p->next = this;
      return;
    }
    p = p->next;
  }
  if (p->next == NULL) {
    p->next = this;
    return;
  }
  next = p->next;
  p->next = this;
}

void ModuleChain::invokeAll(ModuleCmd cmd) {
  ModuleChain*p = head;
  while (p != NULL) {
    if (p->handler != NULL) {
      p->handler(cmd);
    }
    p = p->next;
  }
}


