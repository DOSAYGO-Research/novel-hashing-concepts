- a set of generator functions that are interrelated, and that generate themselves (so the functions themselves evolve), like Fibonacci but with functions or their outputs, not just values, and not just simple tree relationships but they could be in graph
- convolution (like multiplication, or multiplication tableaux with each row rotated by a different keyed parameter) to combine the P and K stream
- P can be padded into blocks (not prime if blocks are small, otherwise unique factorization can decrypt), or treated entirely as one giant number possibly padded



--

Another idea


P = product(many small primes) 

but not an equality, but actually a representation where we snip the bitstream of P into many small segments, where each segment is a prime (trivilliay any bitstream can be mapped to a small prime). 

Then we can transmit C which is the product of all these, and P is unobtainable unless we know the order, because there's i! possibly orders. For large enough i this is too huge. If P is compressed then we learn little from a disordered sequence. 

so the public key? is C and the private key is the ordering? there could be a way to do an asymmetry in here due to the properties of 

factor order, factors, product : 

factors -> product, product -> factors, factors x factor order  : all easy and fast

factors, product -> factor order : impossible

so there's a kind of 1 way ness that is good

and multiplication convolution is a natural very good mixing function

something there


- 
