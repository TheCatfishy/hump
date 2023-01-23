
/* sequential

 * Copyright 2010 Vrije Universiteit
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package ida.ipl;

import java.io.File;
import ibis.ipl.Ibis;
import ibis.ipl.IbisCapabilities;
import ibis.ipl.IbisFactory;
import ibis.ipl.IbisIdentifier;
import ibis.ipl.PortType;
import ibis.ipl.ReadMessage;
import ibis.ipl.ReceivePort;
import ibis.ipl.SendPort;
import ibis.ipl.WriteMessage;
import java.io.IOException;
import java.util.*;   
import java.io.Serializable;
import java.io.Externalizable;


public class Ida {
    Double DepthFound = Double.POSITIVE_INFINITY;
    Double CurrentDepth = 0.0;
    int solvedCounter = 0;
    List<Board> boardList = new ArrayList<>();
    Queue<BoardAndDepth> boardQueue = new LinkedList<>();
    static int expansions;
    Double maxDepth = Double.POSITIVE_INFINITY;
    int bound = 0;

    public class DepthAndSolutions implements Serializable{
        public int solutions;
        public int depth;
        public int found;

        public DepthAndSolutions(int solutions, int depth, int found) {
            this.solutions = solutions;
            this.depth = depth;
            this.found = found;
        }
    }

    public class DepthAndSolutions2 implements Serializable{
        public Integer solutions = 0;
        public Integer depth = 0;
        public Integer found = 0;
    }

    public class BoardAndDepth implements Serializable{
        public Board board;
        public Double depth;

        public BoardAndDepth(Board board, Double depth) {
            this.board = board;
            this.depth = depth;
        }
    }

    BoardCache cache = new BoardCache();
    public static final boolean PRINT_SOLUTION = false;

    PortType portType = new PortType(PortType.COMMUNICATION_RELIABLE,
            PortType.SERIALIZATION_OBJECT, PortType.RECEIVE_EXPLICIT,
            PortType.CONNECTION_MANY_TO_MANY, PortType.RECEIVE_POLL);

    IbisCapabilities ibisCapabilities = new IbisCapabilities(
            IbisCapabilities.ELECTIONS_STRICT, IbisCapabilities.MEMBERSHIP_TOTALLY_ORDERED, IbisCapabilities.CLOSED_WORLD);

    private static int solutions(Board board, int offset) {
        expansions++;
		if (board.distance() == 0) {
			return 1;
		}

		if (board.distance() > board.bound()+ offset) {
			return 0;
		}

		Board[] children = board.makeMoves();
		int result = 0;

		for (int i = 0; i < children.length; i++) {
			if (children[i] != null) {
				result += solutions(children[i], offset);
			}
		}
		return result;
	}


    private int solveThreeDeep(Board board, int worldSize) {
        if(board.distance() == 0)
        {
            return 1;
        }
        boolean solved = false;
        Double curPos = 0.0;
        boardQueue.add(new BoardAndDepth(board, 0.0));      
        int returnDepth = 4;
        Double distribution = 1.1;
        while((!solved || curPos <= DepthFound) && boardQueue.size() > 0){

            if(boardQueue.peek().depth == returnDepth){
                int compute = (boardQueue.size() % worldSize == 0)? 0 : 1;
                 double computeDistributionRatio = ((double) boardQueue.size() / worldSize+compute)/((double) boardQueue.size() / worldSize);
                if(distribution > computeDistributionRatio){
                    return 0;
                }
                else{
                    returnDepth += 1;
                }
            }

            BoardAndDepth front = boardQueue.remove();
            curPos = front.depth;
            Board[] children = front.board.makeMoves();
            for (Board selectedChild : children) {
                if(selectedChild != null){
                    if(selectedChild.distance() == 0){
                        if(front.depth < DepthFound){
                            solvedCounter = 1;
                            DepthFound = front.depth;
                            solved = true;
                        } else if(CurrentDepth == DepthFound){
                            solvedCounter += 1;
                            solved = true;
                        }
                    }else{
                        boardQueue.add(new BoardAndDepth(selectedChild, front.depth + 1));
                    }
                }
            }
        }
        return 0;
    }



    

    

    private void server(Ibis myIbis, IbisIdentifier server, String args[]) throws IOException, Exception {
        //start counting
        
        ReadMessage r = null;
        //establish connections with clients
        int worldSize = myIbis.registry().getPoolSize();
        IbisIdentifier[] clients = myIbis.registry().joinedIbises();
        IbisIdentifier client = null;

        SendPort[] senders = new SendPort[worldSize - 1];

        ReceivePort receiver = myIbis.createReceivePort(portType, "server");

        receiver.enableConnections();
        for(int i = 0, x = 0; i < worldSize; i++)
        {
            client = clients[i];
            if(!client.equals(myIbis.identifier())) 
            {
                senders[x] = myIbis.createSendPort(portType);
                senders[x].connect(client, "client");
                x++;
            }
        }
        //initialize variables
        int clientSize = senders.length;

        String fileName = null;
        boolean cache = true;
        String saveFile = null;
        Object clientInputData = null;

        /* Use suitable default value. */
        int length = 103;

        for (int i = 0; i < args.length; i++) {
            if (args[i].equals("--file")) {
                fileName = args[++i];
            } else if (args[i].equals("--nocache")) {
                cache = false;
            } else if (args[i].equals("--length")) {
                i++;
                length = Integer.parseInt(args[i]);
            } else if (args[i].equals("-o")) {
                i++;
                saveFile = args[i];
            } else {
                System.err.println("No such option: " + args[i]);
                System.exit(1);
            }
        }



        Board initialBoard = null;

        if (fileName == null) {
            initialBoard = new Board(length);
        } else {
            try {
                initialBoard = new Board(fileName);
            } catch (Exception e) {
                System.err
                        .println("could not initialize board from file: " + e);
                System.exit(1);
            }
        }
        if (saveFile != null) {
            System.out.println("Save board to " + saveFile);
            try {
                initialBoard.save(saveFile);
            } catch (java.io.IOException e) {
                System.err.println("Cannot save board: " + e);
            }
            System.exit(0);
        }

        System.out.println("Running IDA*, initial board:");
        System.out.println(initialBoard);

        long start = System.currentTimeMillis();

        int bound = initialBoard.distance();
        int solutions;

        int depth = 0;
        bound = 93;
        initialBoard.setBound(93);
        solutions = solveThreeDeep(initialBoard, worldSize);
        System.out.print("Try bound ");
		System.out.flush();
        
        

        // data how to split the job
        int queueSize = boardQueue.size();
        int quotient = queueSize / worldSize;
        int remainder = queueSize % worldSize;

        Queue<BoardAndDepth> copyQueue = new LinkedList<>(boardQueue);
        
        if(worldSize > 1){
            for(int worldNr = 0 ; worldNr < worldSize-1; worldNr++){
                ArrayList<Board> sendingList = new ArrayList<Board>();
                if(remainder > 0){
                    sendingList.add(copyQueue.remove().board);
                    remainder = remainder - 1;
                }
                for(int x = 0; x < quotient; x++)
                {
                    sendingList.add(copyQueue.remove().board);
                }
                // Send the message.
                WriteMessage w = senders[worldNr].newMessage();
                w.writeObject(sendingList);
                w.finish();
            }
        }
        int offset = 0;
        boolean notFull = true;
        Queue<DepthAndSolutions> otherNodes = new LinkedList<DepthAndSolutions>();
        DepthAndSolutions givenResult = null;

        while(notFull) {
            if(solutions == 0 && bound + offset <= maxDepth){
                Queue<BoardAndDepth> copy2Queue = new LinkedList<>(copyQueue);
                
                while(copy2Queue.size() > 0){
                    BoardAndDepth front = copy2Queue.remove();
                    solutions += solutions(front.board, offset);
                    do {

                r = receiver.poll();
    
                if(r != null) {
                    try 
                    {
                        clientInputData = r.readObject();
                    } 
                    catch (ClassCastException e)
                    {
                        System.out.println("<P>" + "There was an error doing the query:");
                    }
                    r.finish();
                    int[] myArr = null;
                    myArr = (int[]) clientInputData;
                    
                    givenResult = new DepthAndSolutions(myArr[0], myArr[1], 1);
                    boolean set = false;
                    if(givenResult.solutions > 0 && givenResult.depth < maxDepth){
                        maxDepth = new Double(givenResult.depth);                        
                    }
                    for (DepthAndSolutions item: otherNodes) {
                        if(item.depth == givenResult.depth){
                            item.found += 1;
                            item.solutions += givenResult.solutions;
                            set = true;
                        }
                    }
                    if(!set){
                        otherNodes.add(new DepthAndSolutions(givenResult.solutions, givenResult.depth, 1));
                    }
                }
            }while (r != null);
            boolean smallerSolved = true;
            for (DepthAndSolutions item: otherNodes) {
                if(item.solutions > 0 && item.found == worldSize){
                    int checkDepth = item.depth;
                    solutions = item.solutions;
                    for (DepthAndSolutions item2: otherNodes) {

                        if(item2.depth < checkDepth && item2.found != worldSize){
                            smallerSolved = false;
                        }if(item2.depth < checkDepth && item2.solutions > 0){
                            smallerSolved = false;
                        }
                    }
                    if(smallerSolved){
                        for(int startOffset = 91; startOffset <= checkDepth; startOffset+=2){
                                System.out.print(startOffset + " " );
                                System.out.flush();
                            }
                            System.out.print("\nresult is " + solutions + " solutions of " + (checkDepth) + " steps\n");
                            System.out.flush();
                            long end = System.currentTimeMillis();
                            System.err.println("ida took " + (end - start) + " milliseconds");
                        int[] myArr = {1, checkDepth};
                        for(SendPort sender : senders)
                        {
                            WriteMessage wot = sender.newMessage();
                            wot.writeObject(myArr);
                            wot.finish();
                        }

                        notFull = false;
                        do {
                            r = receiver.poll();
                        }while (r != null);

                        for(SendPort sender : senders)
                        {
                            sender.close();
                        }
                        return;
                    }
                }
            }
                    
                }
                boolean set = false;
                for (DepthAndSolutions item: otherNodes) {
                    if(item.depth == bound + offset){
                        item.found += 1;
                        item.solutions += solutions;
                        set = true;
                    }
                }
                if(!set){
                    otherNodes.add(new DepthAndSolutions(solutions, bound + offset, 1));
                }
                boolean smallerSolved = true;
                for (DepthAndSolutions item: otherNodes) {
                    if(item.solutions > 0 && item.found == worldSize){
                        int checkDepth = item.depth;
                        solutions = item.solutions;
                        for (DepthAndSolutions item2: otherNodes) {
                            if(item2.depth < checkDepth && item2.found != worldSize){
                                smallerSolved = false;
                            }if(item2.depth < checkDepth && item2.solutions > 0){
                                smallerSolved = false;
                            }
                        }
                        if(smallerSolved){
                            int[] myArr = {1, checkDepth};
                            for(int startOffset = 91; startOffset <= checkDepth; startOffset+=2){
                                System.out.print(startOffset + " " );
                                System.out.flush();
                            }
                            System.out.print("\nresult is " + solutions + " solutions of " + (checkDepth) + " steps\n");
                                System.out.flush();
                                long end = System.currentTimeMillis();
                                System.err.println("ida took " + (end - start) + " milliseconds");
                            for(SendPort sender : senders)
                            {
                                WriteMessage wot = sender.newMessage();
                                wot.writeObject(myArr);
                                wot.finish();
                            }

                            notFull = false;
                            do {
                                r = receiver.poll();
                            }while (r != null);

                            for(SendPort sender : senders)
                            {
                                sender.close();
                            }
                            return;
                        }
                    }
                }
                offset += 2;
            }
            

            do {

                r = receiver.poll();
    
                if(r != null) {
                    try 
                    {
                        clientInputData = r.readObject();
                    } 
                    catch (ClassCastException e)
                    {
                        System.out.println("<P>" + "There was an error doing the query:");
                    }
                    r.finish();
                    int[] myArr = null;
                    myArr = (int[]) clientInputData;
                    
                    givenResult = new DepthAndSolutions(myArr[0], myArr[1], 1);
                    boolean set = false;
                    if(givenResult.solutions > 0 && givenResult.depth < maxDepth){
                        maxDepth = new Double(givenResult.depth);                        
                    }
                    for (DepthAndSolutions item: otherNodes) {
                        if(item.depth == givenResult.depth){
                            item.found += 1;
                            item.solutions += givenResult.solutions;
                            set = true;
                        }
                    }
                    if(!set){
                        otherNodes.add(new DepthAndSolutions(givenResult.solutions, givenResult.depth, 1));
                    }
                }
            }while (r != null);

            boolean smallerSolved = true;
            for (DepthAndSolutions item: otherNodes) {
                if(item.solutions > 0 && item.found == worldSize){
                    int checkDepth = item.depth;
                    solutions = item.solutions;
                    for (DepthAndSolutions item2: otherNodes) {
                        if(item2.depth < checkDepth && item2.found != worldSize){
                            smallerSolved = false;
                        }if(item2.depth < checkDepth && item2.solutions > 0){
                            smallerSolved = false;
                        }
                    }
                    if(smallerSolved){
                        int[] myArr = {1, checkDepth};
                        for(int startOffset = 93; startOffset <= checkDepth; startOffset+=2){
                            System.out.print(startOffset + " " );
                            System.out.flush();
                        }
                        System.out.print("\nresult is " + solutions + " solutions of " + (checkDepth) + " steps\n");
                        System.out.flush();
                        long end = System.currentTimeMillis();
                        System.err.println("ida took " + (end - start) + " milliseconds");
                        for(SendPort sender : senders)
                        {
                            WriteMessage wot = sender.newMessage();
                            wot.writeObject(myArr);
                            wot.finish();
                        }

                        notFull = false;
                        do {
                            r = receiver.poll();
                        }while (r != null);

                        for(SendPort sender : senders)
                        {
                            sender.close();
                        }
                        return;
                    }
                }
            }
        }
        


        do {
            r = receiver.poll();
        }while (r != null);

        for(SendPort sender : senders)
        {
            sender.close();
        }

    }

    private void client(Ibis myIbis, IbisIdentifier server) throws IOException, Exception {
        //establishing connection connections
        ReceivePort receiver = myIbis.createReceivePort(portType, "client");
        receiver.enableConnections();
        SendPort sender = myIbis.createSendPort(portType);
        sender.connect(server, "server");
        //initialize required variables.
        int totalTwists = 0;
        int offset = 0;
        Queue<Board> receivedQueue = new LinkedList<>();
        Object clientInputData = null;
        ReadMessage r = null;
        ArrayList<Board> sendingList = new ArrayList<Board>();
        int solutions = 0;
        int bound = 93;
        WriteMessage w = null;

        while (r == null) {

            r = receiver.poll();

            if(r != null) {
                try 
                {
                    clientInputData = r.readObject();
                } 
                catch (ClassCastException e)
                {
                    System.out.println("<P>" + "There was an error doing the query:");
                }
                r.finish();
                sendingList = (ArrayList<Board>) clientInputData;
            }
        }
        

        if (sendingList.size() == 0)
        {
            do{
                r = receiver.poll();
            }while(r != null);
            // Close ports.
            sender.close();
            receiver.close();
            return;
        }
        for (Board t : sendingList) {
            // Add each element into the lL
            receivedQueue.add(t);
        }

        while(bound + offset <= maxDepth){
            Queue<Board> copyQueue = new LinkedList<>(receivedQueue);


            while(copyQueue.size() > 0){
                Board front = copyQueue.remove();
                solutions += solutions(front, offset);
                
            }
            if(solutions > 0)
            {
                maxDepth = new Double(bound + offset);
            }
            
            do{
                DepthAndSolutions givenResult = null;
                r = receiver.poll();
    
                if(r != null) {
                    try 
                    {
                        clientInputData = r.readObject();
                    } 
                    catch (ClassCastException e)
                    {
                        System.out.println("<P>" + "There was an error doing the query:");
                    }
                    r.finish();
                    int[] recArr = null;
                    recArr = (int[]) clientInputData;
                    givenResult = new DepthAndSolutions(1, recArr[1], 1);
                    if(givenResult.solutions > 0 && givenResult.depth < maxDepth){
                        maxDepth = new Double(givenResult.depth);        

                    }
                    
                    Double testWolla3 = new Double(bound + offset);
                    if(givenResult.solutions > 0 && testWolla3 > maxDepth){
                        maxDepth = new Double(givenResult.depth);
                        do{
                            r = receiver.poll();
                        }while(r != null);
                        sender.close();
                        receiver.close();
                        return;
                    }
                }
            }while(r != null);
            
            int[] myArr  = {solutions, bound+offset};
            w = sender.newMessage();
            w.writeObject(myArr);
            w.finish();


            offset += 2;
        }
       do{
            r = receiver.poll();
        }while(r != null);
        // Close ports.
        sender.close();
        receiver.close();
    }

    private void run(String args[]) throws Exception {
        // Create an ibis instance.
        Ibis ibis = IbisFactory.createIbis(ibisCapabilities, null, portType);
        
        // Elect a server
        IbisIdentifier server = ibis.registry().elect("Server");

        ibis.registry().waitUntilPoolClosed();

        // If I am the server, run server, else run client.
        if (server.equals(ibis.identifier())) {
            server(ibis, server, args);
        } else {
            client(ibis, server);
        }

        // End ibis.
        ibis.end();
    }

    public static void main(String args[]) {
        try {
            new Ida().run(args);
        } catch (Exception e) {
            e.printStackTrace(System.err);
        }
    }
}

