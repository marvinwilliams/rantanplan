(define (domain test)
(:requirements :strips :typing :equality :negative-preconditions)
(:types thing)
(:constants )
(:predicates 
	(is ?t - thing)
)

(:action test
  :parameters (?t1 ?t2 - thing)
  :precondition (not (and (is ?t1) (not (is ?t2)) (or (and (is ?t2) (not (is ?t1)) (not (and (is ?t1) (is ?t2)))))))
  :effect (and (and (is ?t1) (and (is ?t2) (not (is ?t1)))))
  )

(:action test2
  :parameters (?t1 ?t2 - thing)
  :precondition (or (is ?t1) (not (is ?t2)) (not (or (is ?t1) (not (is ?t2)))))
  :effect (and (is ?t1) (not (is ?t2)) (and (is ?t1) (not (is ?t2))))
  )

(:action test3
  :parameters (?x ?y - thing)
  :precondition (or())
  :effect (not(is ?x))
)

)
