#include "../src/shared_defs.h"
#include "../src/process_table.h"
#include "unity/unity.h"
// use "puts" on occasion for debuging.
#include <stdio.h>

// @Author: Luiz Carlos Rumbelsperger Viana
// Using unity test framework, from http://www.throwtheswitch.org/unity

void setUp(void) { }
  
void tearDown(void) { }

void test_set_and_get_quantum(void){
    // create table and declare new quantum
    ProcessTable table = create_table();
    unsigned short quantum = 1000;

    // old quantum must match default.
    TEST_ASSERT_EQUAL_UINT16(QUANTUM, getQuantum(table));

    // create and insert new ROUND-ROBIN process.
    Process p = create_process("whatever", ROUND_ROBIN | SET_ROBIN_TIME(quantum));
    if (insertProcess(table, p, 0, 0, 0)) 
        TEST_FAIL_MESSAGE("No preemption should have occured");
    
    // assert that new quantum matches quantum of the added process,
    // and that the next process to run is the added process.
    TEST_ASSERT_EQUAL_INT16(quantum, getQuantum(table));
    Process next = next_process(table, 0);
    TEST_ASSERT_EQUAL_PTR(p, next);
    // free everything.
    free_table(table);
}

void test_set_and_get_ran(void){
    ProcessTable table = create_table();

    // create and insert REAL-TIME process.
    Process p = create_process("real", REAL_TIME | SET_D(10) | SET_I(20));
    if (insertProcess(table, p, 0, 0, 0)) 
        TEST_FAIL_MESSAGE("No preemption should have occured");

    // create process to run after the first
    // We run a small policy test as well, because why not?
    unsigned short policy = REAL_TIME | SET_D(5) | SET_I(30);
    TEST_ASSERT_EQUAL_UINT8(30, GET_I(policy));
    TEST_ASSERT_EQUAL_UINT8(5, GET_D(policy));
    TEST_ASSERT_FALSE(POLICY_MAKES_REFERENCE(policy));
    Process other = create_process("/something/else", policy);
    if (insertProcess(table, other, 0, 1, 0)) // added after 1 second.
        TEST_FAIL_MESSAGE("No preemption should have occured in 'other' process");


    // test that at time 0, no processes are allowed to run
    Process next = next_process(table, 0);
    TEST_ASSERT_NULL(next);

    // test that at time 19, no processes are allowed to run
    next = next_process(table, 19);
    TEST_ASSERT_NULL(next);

    // test that at time 20, p is allowed to run
    next = next_process(table, 20);
    TEST_ASSERT_EQUAL_PTR(p, next);

    // test that at time 23, p is allowed to run
    next = next_process(table, 23);
    TEST_ASSERT_EQUAL_PTR(p, next);

    // test that at time 30 it is the other process that is allowed to run
    next = next_process(table, 30);
    TEST_ASSERT_EQUAL_PTR(other, next);

    // test that at time 31 it is the other process that is allowed to run
    next = next_process(table, 31);
    TEST_ASSERT_EQUAL_PTR(other, next);

    // test that at time 35 no process is allowed to run
    next = next_process(table, 35);
    TEST_ASSERT_NULL(next);

    // Suppose now process p has exited at second 23
    // it is then the other process that should be allowed to run
    setRan(table, p);
    next = next_process(table, 23);
    TEST_ASSERT_EQUAL_PTR(other, next);

    // check that it really cant run
    TEST_ASSERT_TRUE(getRan(table, p));
    // check that the other process is not marked as having ran
    TEST_ASSERT_FALSE(getRan(table, other));

    // Suppose now we go to the next minute, by reseting the table.
    // Then we want process p to still be able to run at 20, but not before 20.
    reset(table);
    
    // test that at time 0, no processes are allowed to run
    next = next_process(table, 0);
    TEST_ASSERT_NULL(next);

    // test that at time 19, no processes are allowed to run
    next = next_process(table, 19);
    TEST_ASSERT_NULL(next);

    // test that at time 20, p is allowed to run
    next = next_process(table, 20);
    TEST_ASSERT_EQUAL_PTR(p, next);

    // test that at time 23, p is allowed to run
    next = next_process(table, 23);
    TEST_ASSERT_EQUAL_PTR(p, next);

    // just free everything now
    free_table(table);
}

void test_referential_process_resolves_correctly_and_runs_right_after(void){
    ProcessTable table = create_table();

    // create and insert REAL-TIME process with relative pathname TO RUN IMMEDIATELY.
    Process p1 = create_process("./real", REAL_TIME | SET_D(5) | SET_I(0));
    if (insertProcess(table, p1, 0, 1, 0)) // read after 1 second.
        TEST_FAIL_MESSAGE("No preemption should have occured");
    
    // create and insert REAL-TIME process with absolute pathname to run after 20 secs.
    Process p2 = create_process("/bin/cat", REAL_TIME | SET_D(5) | SET_I(20));
    if (insertProcess(table, p2, 0, 5, 0)) // read after 5 seconds.
        TEST_FAIL_MESSAGE("No preemption should have occured");
    
    Process ref1 = create_process_with_relative_schedule("/bin/bash", "./real", REAL_TIME | SET_D(5));


    // MAKES_REFERENCE flag is supposed to be set automatically. Lets check this.
    TEST_ASSERT_TRUE(POLICY_MAKES_REFERENCE(policy(ref1)));

    if (insertProcess(table, ref1, policy(p1), 2, 0)) // added after 2 seconds, when p1 was running.
        TEST_FAIL_MESSAGE("No preemption should have occured");

    Process ref2 = create_process_with_relative_schedule("/bin/echo", "/bin/cat", REAL_TIME | SET_D(5));
    if (insertProcess(table, ref2, policy(ref1), 6, 0)) // read after 6 seconds with ref1 running
        TEST_FAIL_MESSAGE("No preemption should have occured");

    
    // Lets create a ROUND-ROBIN process to run in between.
    Process robin = create_process("fortune", ROUND_ROBIN);
    if (insertProcess(table, robin, 0, 0, 0)) // read immediately
        TEST_FAIL_MESSAGE("No preemption should have occured");
    
    // When ref1 was added it should have been resolved.

    int i;
    Process next;

    // test that p1 runs between seconds 0 through 4 inclusive
    for (i = 0; i < 5; i++){
        // test that at time i, p1 is allowed to run
        next = next_process(table, i);
        TEST_ASSERT_EQUAL_PTR(p1, next);
    }
    
    // test that ref1 runs between seconds 5 through 9 inclusive
    for (i = 5; i < 10; i++){
        // test that at time i, ref1 is allowed to run
        next = next_process(table, i);
        TEST_ASSERT_EQUAL_PTR(ref1, next);
    }

    // test that robin runs between seconds 10 through 19 inclusive.
    for (i = 10; i < 20; i++){
        // test that at time i, robin is allowed to run
        next = next_process(table, i);
        TEST_ASSERT_EQUAL_PTR(robin, next);
    }

    // test that p2 runs between seconds 20 through 24 inclusive.
    for (i = 20; i < 25; i++){
        // test that at time i, p2 is allowed to run
        next = next_process(table, i);
        TEST_ASSERT_EQUAL_PTR(p2, next);
    }

    // test that ref2 runs between seconds 25 through 29 inclusive.
    for (i = 25; i < 30; i++){
        // test that at time i, ref2 is allowed to run
        next = next_process(table, i);
        TEST_ASSERT_EQUAL_PTR(ref2, next);
    }

    // test that robin runs between seconds 30 through 60 inclusive.
    for (i = 30; i < 61; i++){
        // test that at time i, robin is allowed to run
        next = next_process(table, i);
        TEST_ASSERT_EQUAL_PTR(robin, next);
    }

    // just free everything now
    free_table(table);

}

void test_priority_and_robin(void){

}

void test_error_messages_fire(void){

}

void test_preemption_occurs(void){

}

void test_levels_which_run_too_long_no_longer_run_until_next_minute(void){

}

int main(void){
    UNITY_BEGIN();
    RUN_TEST(test_set_and_get_quantum);
    RUN_TEST(test_set_and_get_ran);
    RUN_TEST(test_referential_process_resolves_correctly_and_runs_right_after);
    return UNITY_END();
}