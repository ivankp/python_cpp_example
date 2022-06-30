#!/usr/bin/env python3

from test_module import string_vector, strlen

print(strlen("test"))

sv = string_vector('hello','world')

print(len(sv),'items')
for x in sv:
    print(x)

print(sv[0])
sv[0] = 'test'
# sv[0] = 1

for x in sv:
    print(x)

print(sv)
print(sv.front())
