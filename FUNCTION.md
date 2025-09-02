There are a few simple steps to how we do functions and lexicalscopes.

In static analysis, during parse and analysis, we determine the lexical scope "depth" at which the variable is accessed and create a direct reference to the "variablinfo" in the lexicalscope in which it is defined.

Once we go through everything, we then compute the lexicalscopes needed for each scope and ALL it's descendants. further we sort them according to access frequency in each scope, most frequently accessd first. This order can be different of course for different depths. some functions won't need any parent scopes.

When we call a function, we pass the scope addresses needed as "hidden" parameters after the parameters of the function. Obviously we might need to save some registers to the stack and restore in epilogue. But the paremters would be like [a,b,...lexicalscopeaddresses].  We get these as follows

a) The current scope address is always stored in r15
b) The other scope addresses were passed as hidden parameters so it would be just like we would handle explicit parameters

We will take care to note that the order of addresses is different for each function due to frequency of usage, and will take care to map them correctly. They are not necessarily in order of depth.

Then it allocates it's own lexicalscope to the stack and stores this address to r15. Obviously would need to save r15 if not root scope level 1.

Then it will generate the function code to access variables normally. variables in lexical scope would be [lexical_scope_register+offset], the offset is available in varaibleinfo poitner object i think. if past 8th parameter, would have extra inderection

Then in epiloguge, it would restore r15,and handle other vars as usual.
