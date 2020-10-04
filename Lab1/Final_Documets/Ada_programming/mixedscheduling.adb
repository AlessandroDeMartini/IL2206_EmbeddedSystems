<<<<<<< HEAD:Program_done/rms21.adb
pragma Priority_Specific_Dispatching(FIFO_Within_Priorities, 10, 50);
pragma Priority_Specific_Dispatching(Round_Robin_Within_Priorities, 1, 9);
=======
-- Add priority specification -> FIFO for main tasks & Round_Robin for background tasks
pragma Priority_Specific_Dispatching(FIFO_Within_Priorities, 15, 60);
pragma Priority_Specific_Dispatching(Round_Robin_Within_Priorities, 1, 1);
>>>>>>> 0ba811a84c3a583cf13842e0dd76e56572c933ea:Lab1/Final_Documets/Ada_programming/mixedscheduling.adb

with Ada.Text_IO; use Ada.Text_IO;
with Ada.Float_Text_IO;

with Ada.Real_Time; use Ada.Real_Time;

procedure Mixedscheduling is

   	package Duration_IO is new Ada.Text_IO.Fixed_IO(Duration);
   	package Int_IO is new Ada.Text_IO.Integer_IO(Integer);
	
   	Start : Time;                          -- Start Time of the System
	Calibrator: constant Integer   := 650; -- Calibration for correct timing
	                                       -- ==> Change parameter for your architecture!
	Warm_Up_Time: constant Integer := 100; -- Warmup time in milliseconds
	
<<<<<<< HEAD:Program_done/rms21.adb
	
=======
>>>>>>> 0ba811a84c3a583cf13842e0dd76e56572c933ea:Lab1/Final_Documets/Ada_programming/mixedscheduling.adb
	-- Conversion Function: Time_Span to Float
	function To_Float(TS : Time_Span) return Float is
        SC   : Seconds_Count;
        Frac : Time_Span;
   	begin
		Split(Time_Of(0, TS), SC, Frac);
		return Float(SC) + Time_Unit * Float(Frac/Time_Span_Unit);
   	end To_Float;
	
	-- Function F is a dummy function that is used to model a running user program.

	function F(N : Integer) return Integer;

   	function F(N : Integer) return Integer is
    	X : Integer := 0;
   	begin
      	for Index in 1..N loop
        	for I in 1..500 loop
            	X := X + I;
         	end loop;
      	end loop;
      	return X;
   end F;
	
	-- Workload Model for a Parametric Task
   	task type T(Id: Integer; Prio: Integer; Phase: Integer; Period : Integer; 
				Computation_Time : Integer; Relative_Deadline: Integer) is
      	pragma Priority(Prio); -- A number between 10 & 50 give FIFO priority

   	end;

   	task body T is
      	Next              : Time;
		Release           : Time;
		Completed         : Time;
		Response          : Time_Span;
		Average_Response  : Float;
		Absolute_Deadline : Time;
		WCRT              : Time_Span; -- measured WCRT (Worst Case Response Time)
     	Dummy             : Integer;
		Iterations        : Integer;
   	begin

		-- Initial Release - Phase
		Release          := Clock + Milliseconds(Phase);
		delay until Release;
		Next             := Release;
		Iterations       := 0;
		Average_Response := 0.0;
		WCRT             := Milliseconds(0);

      	loop
         	Next := Release + Milliseconds(Period);
			Absolute_Deadline := Release + Milliseconds(Relative_Deadline);

         	-- Simulation of User Function
			for I in 1..Computation_Time loop
				Dummy := F(Calibrator); 
			end loop;	

			Completed := Clock;
			Response := Completed - Release;
			Average_Response := (Float(Iterations) * Average_Response + To_Float(Response)) / Float(Iterations + 1);

			if Response > WCRT then
				WCRT := Response;
			end if;

			Iterations := Iterations + 1;			
			Put("Task   ");
			Int_IO.Put(Id, 1);
			Put("- Release: ");
			Duration_IO.Put(To_Duration(Release - Start), 2, 3);
			Put(", Completion: ");
			Duration_IO.Put(To_Duration(Completed - Start), 2, 3);
			Put(", Response: ");
			Duration_IO.Put(To_Duration(Response), 1, 3);
			Put(", WCRT: ");
			Ada.Float_Text_IO.Put(To_Float(WCRT), fore => 1, aft => 3, exp => 0);	
			Put(", Next Release: ");
			Duration_IO.Put(To_Duration(Next - Start), 2, 3);

			if Completed > Absolute_Deadline then 
				Put(" ==> Task ");
				Int_IO.Put(Id, 1);
				Put(" violates Deadline!");
			end if;
         	
			Put_Line("");
			Release := Next;
         	delay until Release;
      	end loop;
   	end T;

	-- Workload Model for a Background Task
	task type BT(Id: Integer; Prio : Integer; Phase: Integer; 
				Computation_Time : Integer) is
<<<<<<< HEAD:Program_done/rms21.adb

		pragma Priority(Prio); -- A number between 1 & 9 give Round Robin priority

   	end BT;
=======
		pragma Priority(Prio);
	end BT;
>>>>>>> 0ba811a84c3a583cf13842e0dd76e56572c933ea:Lab1/Final_Documets/Ada_programming/mixedscheduling.adb

   	task body BT is
      	Next              : Time;
		Release           : Time;
		Completed         : Time;
		Response          : Time_Span;
		Average_Response  : Float;
		WCRT              : Time_Span; -- measured WCRT (Worst Case Response Time)
     	Dummy             : Integer;
		Iterations        : Integer;
   		Period            : Integer := Computation_Time; -- The Task is repeted repitedelly
		begin

			-- Initial Release - Phase
			Release          := Clock + Milliseconds(Phase);
			delay until Release;
			Next             := Release;
			Iterations       := 0;
			Average_Response := 0.0;
			WCRT             := Milliseconds(0);

      		loop
        	 	Next := Release + Milliseconds(Period);
				-- Absolute_Deadline := Release + Milliseconds(Relative_Deadline);

        	 	-- Simulation of User Function
				for I in 1..Computation_Time loop
					Dummy := F(Calibrator); 
				end loop;	

				Completed        := Clock;
				Response         := Completed - Release;
				Average_Response := (Float(Iterations) * Average_Response + To_Float(Response)) / Float(Iterations + 1);

				if Response > WCRT then
					WCRT := Response;
				end if;

				Iterations := Iterations + 1;			
				Put("B_Task ");
				Int_IO.Put(Id, 1);
				Put("- Release: ");
				Duration_IO.Put(To_Duration(Release - Start), 2, 3);
				Put(", Completion: ");
				Duration_IO.Put(To_Duration(Completed - Start), 2, 3);
				Put(", Response: ");
				Duration_IO.Put(To_Duration(Response), 1, 3);
				Put(", WCRT: ");
				Ada.Float_Text_IO.Put(To_Float(WCRT), fore => 1, aft => 3, exp => 0);	
				Put(", Next Release: ");
				Duration_IO.Put(To_Duration(Next - Start), 2, 3);

				Put_Line("");
				Release := Next;
        	 	delay until Release;
      		end loop;
   	end BT;

   	-- Running Tasks
	-- NOTE: All tasks should have a minimum phase, so that they have the same time base!
	
   	Task_1 : T(1, 40, Warm_Up_Time, 300, 100, 300); 	-- ID: 1
	                                            	    -- Priority: 40
                                                        -- Phase: Warm_Up_Time (100)
	                                                    -- Period 300, 
	                                                    -- Computation Time: 100 (if correctly calibrated) 
	                                                    -- Relative Deadline: 300
	Task_2 : T(2, 30, Warm_Up_Time, 400, 100, 400);
	Task_3 : T(3, 20, Warm_Up_Time, 600, 100, 600);
<<<<<<< HEAD:Program_done/rms21.adb
    Task_4 : T(4, 11, Warm_Up_Time, 1200, 200, 1200);

	Taskb_1 : BT(1, 8, 0, 100);
	Taskb_2 : BT(2, 6, 0, 100);
	Taskb_3 : BT(3, 2, 0, 100);

=======
    
	TaskB_1 : BT(1, 1, 1, 100);
	TaskB_2 : BT(2, 1, 1, 100);
	TaskB_3 : BT(3, 1, 1, 100);
>>>>>>> 0ba811a84c3a583cf13842e0dd76e56572c933ea:Lab1/Final_Documets/Ada_programming/mixedscheduling.adb

-- Main Program: Terminates after measuring start time	
begin
   Start := Clock; -- Central Start Time
   null;
end Mixedscheduling;
