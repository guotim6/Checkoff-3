# Checkoff-3
My topic for checkoff 3 is the application of bitwise operators in chess. I learned that chess is represented by something called bitboards, which are sequences of bits with each bit representing a square of the board. At first, I mainly explored how chess mechanics were implemented. I learned how moving and capturing pieces, checks, checkmate, and some special rules of chess such as castling and En Passant were implemented with bitwise operators. Once I had an understanding of the basics, I looked into how bitboards might have helped Stockfish look through chess positions. 

The most interesting discovery I made was that each type of piece has its own bitboard. I initially thought that the pieces would have their type, color, and location encoded in bits. However, chess pieces only have their location represented by the bitboard and the only way to identify the type and color of piece is by variable name. I also found the efficiency of bitboards to be interesting. It allows many chess mechanics like moving to be done in a few operations. For example, moving a pawn forward only requires you to clear the bit and set the bit in front of it. If you wanted to know the possible moves of all pawns, you can shift all of the bits by 8 to move them up one rank (if nothing is blocking the pawns). This ability to calculate possible moves for multiple pieces at once helps Stockfish go through millions of scenarios in an instant. 

## Code Example

## Slides

## Additional Resources
