# Functors

Up until now, we've seen OCaml's module system play an important but
limited role.  In particular, we've seen them as a mechanism for
organizing code into units with specified interfaces.  But modules can
do much more than that, acting as a powerful toolset for building
generic code and structuring large-scale systems.  Much of that power
comes from functors.

Functors are, roughly speaking, functions from modules to modules, and
they can be used to solve a variety of code-structuring problems,
including:

* _Dependency injection_, or making the implementations of some
  components of a system swappable.  This is particularly useful when
  you want to mock up parts of your system for testing and simulation
  purposes.
* _Auto-extension of modules_.  Sometimes, there is some functionality
  that you want to build in a standard way for different types, in
  each case based on a some piece of type-specific logic.  For
  example, you might want to add a slew of comparison operators
  derived from a base comparison function.  To do this by hand would
  require a lot of repetitive code for each type, but functors let you
  write this logic just once and apply it to many different types.
* _Instantiating modules with state_.  Modules can contain mutable
  state, and that means that you'll occasionally want to have multiple
  instantiations of a particular module, each with its own separate
  and independent mutable state.  Functors let you automate the
  construction of such modules.

### A trivial example

We'll start by considering the simplest possible example: a functor
for incrementing an integer.

More precisely, we'll create a functor that takes a module containing
a single integer variable `x`, and returns a new module with `x`
incremented by one.

```ocaml
# module type X_int = sig val x : int end;;
module type X_int = sig val x : int end
# module Increment (M:X_int) : X_int = struct
    let x = M.x + 1
  end;;
module Increment : functor (M : X_int) -> X_int
```

One thing that immediately jumps out about functors is that they're
considerably more heavyweight syntactically than ordinary functions.
For one thing, functors require explicit (module) type annotations,
which ordinary functions do not.  Here, we've specified the module
type `X_int` for both the input and output of the functor.
Technically, only the type on the input is mandatory, although in
practice, one often specifies both.

The following shows what happens when we omit the module type for the
output of the functor.

```ocaml
# module Increment (M:X_int) = struct
    let x = M.x + 1
  end;;
module Increment : functor (M : X_int) -> sig val x : int end
```

We can see that the inferred module type of the output is now written
out explicitly, rather than being a reference to the named signature
`X_int`.

We can now use `Increment` to define new modules.

```ocaml
# module Three = struct let x = 3 end;;
  module Three : sig val x : int end
# module Four = Increment(Three);;
module Four : sig val x : int end
# Four.x - Three.x;;
- : int = 1
```

In this case, we applied `Increment` to a module whose signature is
exactly equal to `X_int`.  But we can apply `Increment` to any module
that satisfies the interface `X_int`, in the same way that a the
contents of an `ml` file can satisfy the `mli`.  That means that the
module type can omit some information available in the module, either
by dropping fields or by leaving some fields abstract.  Here's an
example:

```ocaml
# module Three_and_more = struct
    let x = 3
    let y = "three"
  end;;
module Three_and_more : sig val x : int val x_string : string end
# module Four = Increment(Three_and_more);;
module Four : sig val x : int end
```

### A bigger example: computing with intervals

Let's now consider a more realistic example of how to use functors: a
library for computing with intervals.  This library will be
functorized over the type of the endpoints of the intervals and the
ordering of those endpoints.

First we'll define a module type that captures the information we'll
need about the endpoint type.  This interface, which we'll call
`Comparable`, contains just two things: a comparison function, and the
type of the values to be compared.

```ocaml
# module type Comparable = sig
    type t
    val compare : t -> t -> int
  end ;;
module type Comparable = sig type t val compare : t -> t -> int end
```

The comparison function follows the standard OCaml idiom for such
functions, returning `0` if the two elements are equal, a positive
number if the first element is larger than the second, and a negative
number if the first element is smaller than the second.  Thus, we
could rewrite the standard comparison functions on top of `compare` as
shown below.

```ocaml
compare x y < 0     (* x < y *)
compare x y = 0     (* x = y *)
compare x y > 0     (* x > y *)
```

The functor for creating the interval module is shown below.  We
represent an interval with a variant type, which is either `Empty` or
`Interval (x,y)`, where `x` and `y` are the bounds of the interval.

```ocaml
# module Make_interval(Endpoint : Comparable) = struct

    type t = | Interval of Endpoint.t * Endpoint.t
             | Empty

    let create low high =
      if Endpoint.compare low high > 0 then Empty
      else Interval (low,high)

    let is_empty = function
      | Empty -> true
      | Interval _ -> false

    let contains t x =
      match t with
      | Empty -> false
      | Interval (l,h) ->
        Endpoint.compare x l >= 0 && Endpoint.compare x h <= 0

    let intersect t1 t2 =
      let min x y = if Endpoint.compare x y <= 0 then x else y in
      let max x y = if Endpoint.compare x y >= 0 then x else y in
      match t1,t2 with
      | Empty, _ | _, Empty -> Empty
      | Interval (l1,h1), Interval (l2,h2) ->
        create (max l1 l2) (min h1 h2)

  end ;;
module Make_interval :
  functor (Endpoint : Comparable) ->
    sig
      type t = Interval of Endpoint.t * Endpoint.t | Empty
      val create : Endpoint.t -> Endpoint.t -> t
      val is_empty : t -> bool
      val contains : t -> Endpoint.t -> bool
      val intersect : t -> t -> t
    end
```

We can instantiate the functor by applying it to a module with the
right signature.  In the following, we provide the functor input as an
anonymous module.

```ocaml
# module Int_interval =
    Make_interval(struct
      type t = int
      let compare = Int.compare
    end);;
module Int_interval :
  sig
    type t = Interval of int * int | Empty
    val create : int -> int -> t
    val is_empty : t -> bool
    val contains : t -> int -> bool
    val intersect : t -> t -> t
  end
```

When we choose our interfaces that are aligned with the standards of
our libraries, we don't need to construct a custom module to feed to
the functor.  In this case, for example, we can directly use the `Int`
or `String` modules provided by Core.

```ocaml
# module Int_interval = Make_interval(Int) ;;
# module String_interval = Make_interval(String) ;;
```

This works because many modules in Core, including `Int` and `String`,
satisfy an extended version of the `Comparable` signature described
above.  Standardized signatures are generally good practice, both
because they makes functors easier to use, and because they make the
codebase generally easier to navigate.

Now we can use the newly defined `Int_interval` module like any
ordinary module.

```ocaml
# let i1 = Int_interval.create 3 8;;
val i1 : Int_interval.t = Int_interval.Interval (3, 8)
# let i2 = Int_interval.create 4 10;;
val i2 : Int_interval.t = Int_interval.Interval (4, 10)
# Int_interval.intersect i1 i2;;
- : Int_interval.t = Int_interval.Interval (4, 8)
```

This design gives us the freedom to use any comparison function we
want for comparing the endpoints.  We could, for example, create a
type of int interval with the order of the comparison reversed, as
follows:

```ocaml
# module Rev_int_interval =
    Make_interval(struct
      type t = int
      let compare x y = Int.compare y x
    end);;
```

The behavior of `Rev_int_interval` is of course different from
`Int_interval`, as we can see below.

```ocaml
# let interval = Int_interval.create 4 3;;
val interval : Int_interval.t = Int_interval.Empty
# let rev_interval = Rev_int_interval.create 4 3;;
val rev_interval : Rev_int_interval.t = Rev_int_interval.Interval (4, 3)
```

Importantly, `Rev_int_interval.t` is a different type than
`Int_interval.t`, even though its physical representation is the same.
Indeed, the type system will prevent us from confusing them.

```ocaml
# Int_interval.contains rev_interval 3;;
Characters 22-34:
  Int_interval.contains rev_interval 3;;
                        ^^^^^^^^^^^^
Error: This expression has type Rev_int_interval.t
       but an expression was expected of type
         Int_interval.t = Make_interval(Int).t
```

This is important, because confusing the two kinds of intervals would
be a semantic error, and it's an easy one to make.  The ability of
functors to mint new types is a useful trick that comes up a lot.

#### Making the functor abstract

There's a problem with `Make_interval`.  The code we wrote depends on
the invariant that the upper bound of an interval is greater than its
lower bound, but that invariant can be violated.  The invariant is
enforced by the create function, but because `Interval.t` is not
abstract, we can bypass the `create` function.

```ocaml
# Int_interval.create 4 3;; (* going through create *)
- : Int_interval.t = Int_interval.Empty
# Int_interval.Interval (4,3);; (* bypassing create *)
- : Int_interval.t = Int_interval.Interval (4, 3)
```

To make `Int_interval.t` abstract, we need to apply an interface to
the output of the `Make_interval`.  Here's an explicit interface that
we can use for that purpose.

```ocaml
# module type Interval_intf = sig
   type t
   type endpoint
   val create : endpoint -> endpoint -> t
   val is_empty : t -> bool
   val contains : t -> endpoint -> bool
   val intersect : t -> t -> t
  end;;
```

This interface includes the type `endpoint` to represent the type of
the endpoints of the interval.  Given this interface, we can redo our
definition of `Make_interval`.  Notice that we added the type
`endpoint` to the implementation of the module to make the
implementation match `Interval_intf`.

```ocaml
# module Make_interval(Endpoint : Comparable) : Interval_intf = struct

    type endpoint = Endpoint.t
    type t = | Interval of Endpoint.t * Endpoint.t
             | Empty

    ....

  end ;;
module Make_interval : functor (Endpoint : Comparable) -> Interval_intf
```

#### Sharing constraints

The resulting module is abstract, but unfortunately, it's too
abstract.  In particular, we haven't exposed the type `endpoint`,
which means that we can't even construct an interval anymore.

```ocaml
# module Int_interval = Make_interval(Int);;
module Int_interval : Interval_intf
# Int_interval.create 3 4;;
Characters 20-21:
  Int_interval.create 3 4;;
                      ^
Error: This expression has type int but an expression was expected of type
         Int_interval.endpoint
```

To fix this, we need to expose the fact that `endpoint` is equal to
`Int.t` (or more generally, `Endpoint.t`, where `Endpoint` is the
argument to the functor).  One way of doing this is through a _sharing
constraint_, which allows you to tell the compiler to expose the fact
that a given type is equal to some other type.  The syntax for a
sharing constraint on a module type is as follows.

```ocaml
S with type s = t
```

where `S` is a module type, `s` is a type inside of `S`, and `t` is a
type defined outside of `S`.  The result of this expression is a new
signature that's been modified so that it exposes the fact that `s` is
equal to `t`.  We can use a sharing constraint to create a specialized
version of `Interval_intf` for integer intervals.

```ocaml
# module type Int_interval_intf = Interval_intf with type endpoint = int;;
module type Int_interval_intf =
  sig
    type t
    type endpoint = int
    val create : endpoint -> endpoint -> t
    val is_empty : t -> bool
    val contains : t -> endpoint -> bool
    val intersect : t -> t -> t
  end
```

And we can also use it in the context of a functor, where the
right-hand side of the sharing constraint is an element of the functor
argument.  Thus, we expose an equality between a type in the output of
the functor (in this case, the type `endpoint`) and a type in its
input (`Endpoint.t`).

```ocaml
# module Make_interval(Endpoint : Comparable)
      : Interval_intf with type endpoint = Endpoint.t = struct

    type endpoint = Endpoint.t
    type t = | Interval of Endpoint.t * Endpoint.t
             | Empty

    ...

  end ;;
module Make_interval :
  functor (Endpoint : Comparable) ->
    sig
      type t
      type endpoint = Endpoint.t
      val create : endpoint -> endpoint -> t
      val is_empty : t -> bool
      val contains : t -> endpoint -> bool
      val intersect : t -> t -> t
    end
```

So now, the interface is as it was, except that `endpoint` is now
known to be equal to `Endpoint.t`.  As a result of that type equality,
we can now do things like construct intervals again.

```ocaml
# let i = Int_interval.create 3 4;;
val i : Int_interval.t = <abstr>
# Int_interval.contains i 5;;
- : bool = false
```

#### Destructive substitution

Sharing constraints basically do the job, but they have some
downsides.  In particular, we've now been stuck with the useless type
declaration of `endpoint` that clutters up both the interface and the
implementation.  A better solution would be to modify the
`Interval_intf` signature by replacing `endpoint` with `Endpoint.t`
everywhere it shows up, and deleting the definition of `endpoint` from
the signature.  We can do just this using what's called _destructive
substitution_.  Here's the basic syntax.

```ocaml
S with type s := t
```

The following shows how we could use this with `Make_interval`.

Here's an example of what we get if we use destructive substitution to
specialize the `Interval_intf` interface to integer intervals.

```ocaml
# module type Int_interval_intf = Interval_intf with type endpoint := int;;
module type Int_interval_intf =
  sig
    type t
    val create : int -> int -> t
    val is_empty : t -> bool
    val contains : t -> int -> bool
    val intersect : t -> t -> t
  end
```

There's now no mention of n `endpoint`, all occurrences of that type
having been replaced by `int`.  As with sharing constraints, we can
also use this in the context of a functor.

```ocaml
# module Make_interval(Endpoint : Comparable)
    : Interval_intf with type endpoint := Endpoint.t =
  struct

    type t = | Interval of Endpoint.t * Endpoint.t
             | Empty

    ....

  end ;;
module Make_interval :
  functor (Endpoint : Comparable) ->
    sig
      type t
      val create : Endpoint.t -> Endpoint.t -> t
      val is_empty : t -> bool
      val contains : t -> Endpoint.t -> bool
      val intersect : t -> t -> t
    end
```

The interface is precisely what we want, and we didn't need to define
the `endpoint` type alias in the body of the module.  If we
instantiate this module, we'll see that it works properly: we can
construct new intervals, but `t` is abstract, and so we can't directly
access the constructors and violate the invariants of the data
structure.

```ocaml
# module Int_interval = Make_interval(Int);;
# Int_interval.create 3 4;;
- : Int_interval.t = <abstr>
# Int_interval.Interval (4,3);;
Characters 0-27:
  Int_interval.Interval (4,3);;
  ^^^^^^^^^^^^^^^^^^^^^^^^^^^
Error: Unbound constructor Int_interval.Interval
```

#### Using multiple interfaces

Another feature that we might want for our interval module is the
ability to serialize the type, in particular, by converting to
s-expressions.  If we simply invoke the `sexplib` macros by adding
`with sexp` to the definition of `t`, though, we'll get an error:

```ocaml
# module Make_interval(Endpoint : Comparable)
    : Interval_intf with type endpoint := Endpoint.t = struct

    type t = | Interval of Endpoint.t * Endpoint.t
             | Empty
    with sexp

    ....

  end ;;
Characters 120-123:
        type t = | Interval of Endpoint.t * Endpoint.t
                               ^^^^^^^^^^
Error: Unbound value Endpoint.t_of_sexp
```

The problem is that `with sexp` adds code for defining the
s-expression converters, and that code assumes that `Endpoint` has the
appropriate sexp-conversion functions for `Endpoint.t`.  But all we
know about `Endpoint` is that it satisfies the `Comparable` interface, which
doesn't say anything about s-expressions.

Happily, Core comes with a built in interface for just this purpose
called `Sexpable`, which is defined as follows:

```ocaml
module type Sexpable = sig
  type t = int
  val sexp_of_t : t -> Sexp.t
  val t_of_sexp : Sexp.t -> t
end
```

We can modify `Make_interval` to use the `Sexpable` interface, for
both its input and its output.  Note the use of destructive
substitution to combine multiple signatures together.  This is
important because it stops the `type t`'s from the different
signatures from interfering with each other.

Also note that we have been careful to override the sexp-converter
here to ensure that the data structures invariants are still maintained
when reading in from an s-expression.

```ocaml
# module type Interval_intf_with_sexp = sig
   type t
   include Interval_intf with type t := t
   include Sexpable      with type t := t
  end;;
# module Make_interval(Endpoint : sig
    type t
    include Comparable with type t := t
    include Sexpable   with type t := t
  end) : Interval_intf_with_sexp with type endpoint := Endpoint.t =
  struct

      type t = | Interval of Endpoint.t * Endpoint.t
               | Empty
      with sexp

      let create low high =
         ...

      (* put a wrapper round the auto-generated sexp_of_t to enforce
         the invariants of the data structure *)
      let t_of_sexp sexp =
        match t_of_sexp sexp with
        | Empty -> Empty
        | Interval (x,y) -> create x y

      ....

     end ;;
module Make_interval :
  functor
    (Endpoint : sig
           type t
           val compare : t -> t -> int
           val sexp_of_t : t -> Sexplib.Sexp.t
           val t_of_sexp : Sexplib.Sexp.t -> t
         end) ->
    sig
      type t
      val create : Endpoint.t -> Endpoint.t -> t
      val is_empty : t -> bool
      val contains : t -> Endpoint.t -> bool
      val intersect : t -> t -> t
      val sexp_of_t : t -> Sexplib.Sexp.t
      val t_of_sexp : Sexplib.Sexp.t -> t
    end
```

And now, we can use that sexp-converter in the ordinary way:

```ocaml
# module Int = Make_interval(Int) ;;
# Int_interval.sexp_of_t (Int_interval.create 3 4);;
- : Sexplib.Sexp.t = (Interval 3 4)
# Int_interval.sexp_of_t (Int_interval.create 4 3);;
- : Sexplib.Sexp.t = Empty
```

### Extending modules

Another common use of functors is to generate type-specific
functionality for a given module in a standardized way.  Let's see how
this works in the context of a functional queue, which is just a
functional version of a FIFO (first-in, first-out) queue.  Being
functional, operations on the queue return new queues, rather than
modifying the queues that were passed in.

Here's a reasonable mli

```ocaml
(* file: fqueue.mli *)

type 'a t

val empty : 'a t

val enqueue : 'a t -> 'a -> 'a t

(** [dequeue q] returns None if the [q] is empty *)
val dequeue : 'a t -> ('a * 'a t) option

val fold : 'a t -> init:'acc -> f:('acc -> 'a -> 'acc) -> 'acc
```

Now let's implement `Fqueue`.  A standard trick is for the `Fqueue` to
maintain an input and an output list, so that one can efficiently
`enqueue` on the first list, and can efficiently dequeue from the out
list.  If you attempt to dequeue when the output list is empty, the
input list is reversed and becomes the new output list.  Here's an
implementation that uses that trick.

```ocaml
(* file: fqueue.ml *)
open Core.Std

type 'a t = 'a list * 'a list

let empty = ([],[])

let enqueue (l1,l2) x = (x :: l1,l2)

let dequeue (in_list,out_list) =
  match out_list with
  | hd :: tl -> Some (hd, (in_list,tl))
  | [] ->
    match List.rev in_list with
    | [] -> None
    | hd::tl -> Some (hd, ([], tl))

let fold (in_list,out_list) ~init ~f =
  List.fold ~init:(List.fold ~init ~f out_list) ~f
    (List.rev in_list)
```

One problem with our `Fqueue` is that the interface is quite skeletal.
There are lots of useful helper functions that one might want that
aren't there.  For example, for lists we have `List.iter` which runs a
function on each node; and a `List.find` that finds the first element
on the list that matches a given predicate.  Such helper functions
come up for pretty much every container type, and implementing them
over and over is a bit of a dull and repetitive affair.

As it happens, many of these helper functions can be derived
mechanically from just the fold function we already implemented.
Rather than write all of these helper functions by hand for every new
container type, we can instead use a functor that will let us add this
functionality to any container that has a `fold` function.

We'll create a new module, `Foldable` that automates the process of
adding helper functions to a fold-supporting container.  As you can
see, `Foldable` contains a module signature `S` which defines the
signature that is required to support folding; and a functor `Extend`
that allows one to extend any module that matches `Foldable.S`.

```ocaml
(* file: foldable.ml *)

open Core.Std

module type S = sig
  type 'a t
  val fold : 'a t -> init:'acc -> f:('acc -> 'a -> 'acc) -> 'acc
end

module type Extension = sig
  type 'a t
  val iter    : 'a t -> f:('a -> unit) -> unit
  val length  : 'a t -> int
  val count   : 'a t -> f:('a -> bool) -> int
  val for_all : 'a t -> f:('a -> bool) -> bool
  val exists  : 'a t -> f:('a -> bool) -> bool
end

(* For extending a Foldable module *)
module Extend(Arg : S)
  : Extension with type 'a t := 'a Arg.t =
struct
  open Arg

  let iter t ~f =
    fold t ~init:() ~f:(fun () a -> f a)

  let length t =
    fold t ~init:0  ~f:(fun acc _ -> acc + 1)

  let count t ~f =
    fold t ~init:0  ~f:(fun count x -> count + if f x then 1 else 0)

  exception Short_circuit

  let for_all c ~f =
    try iter c ~f:(fun x -> if not (f x) then raise Short_circuit); true
    with Short_circuit -> false

  let exists c ~f =
    try iter c ~f:(fun x -> if f x then raise Short_circuit); false
    with Short_circuit -> true
end
```

Now we can apply this to `Fqueue`.  We can rewrite the interface of
`Fqueue` as follows.

```ocaml
(* file: fqueue.mli *)
open Core.Std

type 'a t
val empty : 'a t
val enqueue : 'a t -> 'a -> 'a t
val dequeue : 'a t -> ('a * 'a t) option
val fold : 'a t -> init:'acc -> f:('acc -> 'a -> 'acc) -> 'acc

include Foldable.Extension with type 'a t := 'a t
```

In order to apply the functor, we'll put the definition of `Fqueue` in
a sub-module called `T`, and then call `Foldable.Extend` on `T`.

```ocaml
open Core.Std

module T = struct
  type 'a t = 'a list * 'a list

  let empty = [],[]

  let enqueue (l1,l2) x =
    (x :: l1,l2)

  let rec dequeue (in_list,out_list) =
    match out_list with
    | hd :: tl -> Some (hd, (in_list,tl))
    | [] -> dequeue ([], List.rev in_list)

  let fold (in_list,out_list) ~init ~f =
    List.fold ~init:(List.fold ~init ~f out_list) ~f
      (List.rev in_list)
end
include T
include Foldable.Extend(T)
```

This pattern comes up quite a bit in Core, and is used to for a
variety of purposes.

- Adding comparison-based data structures like maps and sets, based on
  the `Comparable` interface.
- Adding hash-based data structures like hash sets and hash heaps.
- Support for so-called monadic libraries, like the ones discussed in
  [xref](#error-handling) and
  [xref](#concurrent-programming-with-async).  Here, the functor is
  used to provide a collection of standard helper functions based on
  the core `bind` and `return` operators.
