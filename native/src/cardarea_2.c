// cardarea.lua:632-668: CardArea:load and CardArea:remove.
#include "all.h"

// cardarea.lua:632
void CardArea_load(CardArea *self, Table *cardAreaTable) {
    if (self->cards) remove_all(self->cards);                                   // :634
    self->cards = tnew();                                                       // :635
    if (self->moveable.node.children) remove_all(self->moveable.node.children); // :636
    self->moveable.node.children = tnew();                                      // :637

    self->moveable.node.config = ttab(cardAreaTable, "config"); // :639 the table itself

    Table *saved_cards = ttab(cardAreaTable, "cards");
    for (int i = 1; i <= tlen(saved_cards); i++) { // :641
        // :642/:644 the accidental global `loading` is read by nothing: it does not exist in C (cardarea.h)
        Card *card = NEW(Card);
        Card_init(card, 0, 0, G->CARD_W, G->CARD_H, ttab(G->P_CENTERS, "j_joker"), ttab(G->P_CENTERS, "c_base"), NULL); // :643
        Card_load(card, ttabi(saved_cards, i), NULL); // :645
        tinsert(self->cards, O(card));                // :646
        if (card->highlighted) {                      // :647
            tinsert(self->highlighted, O(card));      // :648 (highlighted is not cleared before, as in the Lua)
        }
        Card_set_card_area(card, self); // :650
    }
    CardArea_set_ranks(self);      // :652
    CardArea_align_cards(self);    // :653
    CardArea_hard_set_cards(self); // :654
}

// cardarea.lua:657
void CardArea_remove(CardArea *self) {
    if (self->cards) remove_all(self->cards); // :658
    self->cards = NULL;                       // :659
    if (self->moveable.node.children) remove_all(self->moveable.node.children); // :660
    // :661 note: the Lua sets self.children = nil; node.h says children is never NULL, so it becomes a new empty
    // table: the same for every reader (remove_all leaves the named children in the old table, and Node_remove must
    // not remove them a second time).
    self->moveable.node.children = tnew();
    // :662 note: pairs() over an array with table.remove inside: after a removal the walk goes on at k + 1, skipping
    // the entry that moved down to k. The same here.
    for (int k = 1; k <= tlen(G->I.CARDAREA); k++) {
        if (tobji(G->I.CARDAREA, k) == (void *)self) { // :663
            tremove(G->I.CARDAREA, k);                 // :664
        }
    }
    Moveable_remove((Moveable *)self); // :667
}
