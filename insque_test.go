package libc

import (
	"testing"
	"unsafe"
)

// mirrors musl's struct node { struct node *next, *prev; } — next@0, prev@8.
type qnode struct {
	next, prev unsafe.Pointer
	val        int
}

func TestInsque(t *testing.T) {
	var a, b, c qnode
	a.val, b.val, c.val = 1, 2, 3
	Insque(unsafe.Pointer(&a), nil)               // a: head
	Insque(unsafe.Pointer(&b), unsafe.Pointer(&a)) // a -> b
	Insque(unsafe.Pointer(&c), unsafe.Pointer(&b)) // a -> b -> c
	if a.next != unsafe.Pointer(&b) || b.prev != unsafe.Pointer(&a) {
		t.Fatal("insque a<->b")
	}
	if b.next != unsafe.Pointer(&c) || c.prev != unsafe.Pointer(&b) {
		t.Fatal("insque b<->c")
	}
	Remque(unsafe.Pointer(&b)) // a -> c
	if a.next != unsafe.Pointer(&c) || c.prev != unsafe.Pointer(&a) {
		t.Fatal("remque relink")
	}
}
