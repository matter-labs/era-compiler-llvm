# Code Review Checklist

<!-- 
MIT License

Copyright (c) 2020 Michaela Greiler
from https://github.com/mgreiler/code-review-checklist/
Modified 2023– by Matter Labs,
some extracts from https://www.codereviewchecklist.com added,
Copyright (c) 2020 Lee Englestone
also MIT License.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
 -->

## Purpose


## Ticket Number


## Requirements
- [ ] Have the requirements been met?
- [ ] Have stakeholder(s) approved the change?

## Implementation
- [ ] Does this code change accomplish what it is supposed to do?
- [ ] Can this solution be simplified?
- [ ] Does this change add unwanted compile-time or run-time dependencies?
- [ ] Could an additional framework, API, library, or service improve the solution?
- [ ] Could we reuse part of LLVM instead of implementing the patch or a part of it?
- [ ] Is the code at the right abstraction level?
- [ ] Is the code modular enough?
- [ ] Can a better solution be found in terms of maintainability, readability, performance, or security?
- [ ] Does similar functionality already exist in the codebase? If yes, why isn’t it reused?
- [ ] Are there any best practices, design patterns or language-specific patterns that could substantially improve this code? 

## Logic Errors and Bugs
- [ ] Can you think of any use case in which the
code does not behave as intended?
- [ ] Can you think of any inputs or external events
that could break the code?

## Error Handling and Logging
- [ ] Is error handling done the correct way?
- [ ] Should any logging or debugging information
be added or removed?
- [ ] Are error messages user-friendly?
- [ ] Are there enough log events and are they
written in a way that allows for easy
debugging?

## Maintainability
- [ ] Is the code easy to read?
- [ ] Is the code not repeated (DRY Principle)?
- [ ] Is the code method/class not too long?

## Dependencies
- [ ] Were updates to documentation, configuration, or readme files made as required by this change?
- [ ] Are there any potential impacts on other parts of the system or backward compatibility?

## Security
- [ ] Does the code introduce any security vulnerabilities?

## Performance
- [ ] Do you think this code change decreases
system performance?
- [ ] Do you see any potential to improve the
performance of the code significantly?

## Testing and Testability
- [ ] Is the code testable?
- [ ] Have automated tests been added, or have related ones been updated to cover the change?
	- [ ] For changes to mutable state
- [ ] Do tests reasonably cover the code change (unit/integration/system tests)? 
	- [ ] Line Coverage
	- [ ] Region Coverage
	- [ ] Branch Coverage
- [ ] Are there some test cases, input or edge cases
that should be tested in addition?

## Readability
- [ ] Is the code easy to understand?
- [ ] Which parts were confusing to you and why?
- [ ] Can the readability of the code be improved by
smaller methods?
- [ ] Can the readability of the code be improved by
different function, method or variable names?
- [ ] Is the code located in the right
file/folder/package?
- [ ] Do you think certain methods should be
restructured to have a more intuitive control
flow?
- [ ] Is the data flow understandable?
- [ ] Are there redundant or outdated comments?
- [ ] Could some comments convey the message
better?
- [ ] Would more comments make the code more
understandable?
- [ ] Could some comments be removed by making the code itself more readable?
- [ ] Is there any commented-out code?

## Documentation
- [ ] Is there sufficient documentation?
- [ ] Is the ReadMe.md file up to date?
- [ ] Have you run a spelling and grammar checker?

## Best Practices
- [ ] Follow Single Responsibility principle?
- [ ] Are different errors handled correctly?
- [ ] Are errors and warnings logged?
- [ ] Magic values avoided?
- [ ] No unnecessary comments?
- [ ] Minimal nesting used?

## Experts' Opinion
- [ ] Do you think a specific expert, like a security
expert or a usability expert, should look over
the code before it can be accepted?
- [ ] Will this code change impact different teams, and should they review the change as well?
