(define (domain test)
(:requirements :strips :typing :equality :negative-preconditions)
(:types thing1)
(:constants a - thing1)
(:predicates
	(is ?t - thing1)
    (done)
)

(:action make
  :parameters (?t - thing1)
  :precondition (not (is ?t))
  :effect (is ?t)
  )
  (:action make2
  :parameters (?t - thing1)
  :precondition (is ?t)
  :effect (done)
  )


)
