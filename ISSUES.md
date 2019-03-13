Below is a list of known issues and quick fixes.  Please check before reporting a bug or struggling to find a solution on the web. [CHECK ACTUAL ERRORS MESSAGES]

**Q. I'm not able to run corpux-analyzer, I get a "/usr/bin/env : bad interpreter : No such file or directory" error ?**

**A.** Replace the "/usr/bin/env" shebang at the top of the corpux-analyzer file by whatever is the result of the "which env" command in a terminal emulator (be sure to KEEP the "#!" characters at the beginning of the line).

**Q. I'm not able to run corpux-analyzer, I get a "cannot found python" ?**

**A.** Install python — preferably 2.6 or more.

**Q. I am using an ISO-8859 or GB2312 or WTF666 locale and getting weird results, what's the matter ?**

**A.** The corpux suite has currently been tested under UTF-8 only.

